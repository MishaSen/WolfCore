// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Presage/PresageSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "WolfCore/Public/Presage/ActorSnapshot.h"
#include "WolfCore/Public/Presage/PresageAbilityRequest.h"
#include "Components/PrimitiveComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/WolfGameplayTags.h"
#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"
#include "TimerManager.h"
#include "AbilitySystem/WolfAttributeSetBase.h"
#include "Character/WolfCharacterBase.h"

void UPresageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UPresageSubsystem::BindToModeSwitchEvent);
	}
}

void UPresageSubsystem::BindToModeSwitchEvent()
{
	if (const UWorld* World = GetWorld())
	{
		if (auto* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerPawn))
			{
				const auto& Tag = FWolfGameplayTags::Get();
				FGameplayTagContainer EventTagContainer;
				EventTagContainer.AddTag(Tag.Event_ModeSwitchReady);

				ASC->AddGameplayEventTagContainerDelegate
				(
					EventTagContainer,
					FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject
					(
						this,
						&ThisClass::OnModeSwitchEventReceived
					)
				);
			}
		}
	}
}

void UPresageSubsystem::OnModeSwitchEventReceived(FGameplayTag GameplayTag, const FGameplayEventData* GameplayEventData)
{
	if (const auto& Tag = FWolfGameplayTags::Get(); GameplayEventData->TargetTags.HasTag(Tag.InputState_TB))
	{
		StartLoop();
	}
	else
	{
		StopLoop();
	}
}

void UPresageSubsystem::Deinitialize()
{
	Super::Deinitialize();
	StopLoop();
}

UPresageSubsystem* UPresageSubsystem::Get(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UPresageSubsystem>();
}

void UPresageSubsystem::Tick(float DeltaTime)
{
	if (!bLoopActive)
	{
		return;
	}

	AccumulatedTime += DeltaTime;
	if (AccumulatedTime >= FlowTime)
	{
		AccumulatedTime -= FlowTime;
		OnFlowTimerTick();
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// Process queued abilities
	for (int32 i = AbilityQueue.Num() - 1; i >= 0; --i)
	{
		if (const FPresageAbilityRequest& Request = AbilityQueue[i]; Request.GetRequestedTime() <= CurrentTime)
		{
			if (auto ASC = Request.GetOwnerASC())
			{
				// TODO: Send request through interface to character for activation.
			}
			AbilityQueue.RemoveAt(i);
		}
	}
}

void UPresageSubsystem::StartLoop()
{
	if (bLoopActive)
	{
		return;
	}

	bLoopActive = true;
	AccumulatedTime = 0.f;

	CharacterSnapshot(); // 1. Save the present
	UpdateTimelinePrediction(); // 2. Calculate the future
	// 3. TODO: Update UI to read 'CurrentPredictedTimeline' and draw icons
}

void UPresageSubsystem::StopLoop()
{
	if (!bLoopActive)
	{
		return;
	}

	bLoopActive = false;
	AccumulatedTime = 0.f;
}

void UPresageSubsystem::OnFlowTimerTick()
{
	RevertCharacterStates();
}

void UPresageSubsystem::QueueAbilityRequest(const FPresageAbilityRequest& Request)
{
	AbilityQueue.Add(Request);
	StartLoop();
}

void UPresageSubsystem::CharacterSnapshot()
{
	OriginalCharacterStates.Empty();

	TArray<AActor*> Characters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWolfCharacterBase::StaticClass(), Characters);

	for (AActor* Actor : Characters)
	{
		AWolfCharacterBase* Char = Cast<AWolfCharacterBase>(Actor);
		if (!Char) continue;

		FActorSnapshot NewSnapshot;
		NewSnapshot.ActorRef = Char;

		// --- 1. Physics Snapshot ---
		NewSnapshot.Location = Char->GetActorLocation();
		NewSnapshot.Rotation = Char->GetActorRotation();
		NewSnapshot.Velocity = Char->GetVelocity();

		if (const UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
		{
			NewSnapshot.MovementMode = MoveComp->MovementMode;
			NewSnapshot.CustomMovementMode = MoveComp->CustomMovementMode;
		}

		// --- 2. GAS Snapshot ---
		auto* ASC = Char->GetAbilitySystemComponent();
		if (!ASC) continue;

		FGameplayTagContainer CurrentTags;
		ASC->GetOwnedGameplayTags(CurrentTags);
		for (const auto& Tag : CurrentTags)
		{
			if (Tag.MatchesTag(FWolfGameplayTags::Get().InputState)) continue;
			NewSnapshot.Tags.AddTag(Tag);
		}

		TArray<FGameplayAttribute> CharAttributes;
		ASC->GetAllAttributes(CharAttributes);

		for (const auto& Attribute : CharAttributes)
		{
			NewSnapshot.Attributes.Add(Attribute, ASC->GetNumericAttributeBase(Attribute));
		}

		FGameplayEffectQuery Query;

		for (auto ActiveHandles = ASC->GetActiveEffects(Query);
		     const auto& Handle : ActiveHandles)
		{
			if (const auto* Effect = ASC->GetActiveGameplayEffect(Handle))
			{
				FStoredEffect StoredEffect;
				StoredEffect.EffectClass = Effect->Spec.Def.GetClass();
				StoredEffect.Level = Effect->Spec.GetLevel();
				StoredEffect.Stacks = Effect->Spec.GetStackCount();
				StoredEffect.RemainingDuration = Effect->GetDuration() > 0.f
					                                 ? Effect->GetTimeRemaining(GetWorld()->GetTimeSeconds())
					                                 : -1.f;

				NewSnapshot.ActiveEffects.Add(StoredEffect);
			}
		}

		// --- 3. Animation Snapshot ---
		if (const UAnimInstance* AInst = Char->GetMesh()->GetAnimInstance())
		{
			if (UAnimMontage* Mon = AInst->GetCurrentActiveMontage())
			{
				NewSnapshot.CurrentMontage = Mon;
				NewSnapshot.MontagePosition = AInst->Montage_GetPosition(Mon);
			}
		}

		OriginalCharacterStates.Add(NewSnapshot);
	}
}

void UPresageSubsystem::RevertCharacterStates()
{
	TArray<AActor*> Characters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWolfCharacterBase::StaticClass(), Characters);
	for (AActor* Actor : Characters)
	{
		AWolfCharacterBase* Char = Cast<AWolfCharacterBase>(Actor);
		if (!Char) continue;

		// --- 1. Physics Revert ---
		const FActorSnapshot* FoundState = OriginalCharacterStates.FindByPredicate([Char](const FActorSnapshot& State)
		{
			return State.ActorRef == Char;
		});
		if (!FoundState) continue;

		Char->SetActorLocationAndRotation(
			FoundState->Location,
			FoundState->Rotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		if (UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
		{
			MoveComp->SetMovementMode(FoundState->MovementMode, FoundState->CustomMovementMode);
			MoveComp->Velocity = FoundState->Velocity;
			MoveComp->UpdateComponentVelocity();
		}

		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Char->GetRootComponent()))
		{
			if (PrimComp->IsSimulatingPhysics())
			{
				PrimComp->SetPhysicsLinearVelocity(FoundState->Velocity);
			}
		}

		// --- 2. GAS Revert ---
		auto* ASC = Char->GetAbilitySystemComponent();
		if (!ASC) continue;

		for (auto& [Attr, AttrValue] : FoundState->Attributes)
		{
			const auto& Attribute = Attr;
			const auto AttributeValue = AttrValue;

			if (FMath::IsNearlyEqual(ASC->GetNumericAttributeBase(Attribute), AttributeValue)) continue;
			ASC->SetNumericAttributeBase(Attribute, AttributeValue);
		}

		FGameplayTagContainer CurrentTags;
		ASC->GetOwnedGameplayTags(CurrentTags);
		for (const auto& Tag : CurrentTags)
		{
			if (Tag.MatchesTag(FWolfGameplayTags::Get().InputState)) continue;
			ASC->RemoveLooseGameplayTag(Tag);
		}

		ASC->AddLooseGameplayTags(FoundState->Tags);

		ASC->RemoveActiveEffects(FGameplayEffectQuery());
		for (const auto& [
			     EffectClass,
			     Level,
			     Stacks,
			     RemainingDuration]
		     : FoundState->ActiveEffects)
		{
			if (!EffectClass) continue;

			const auto Context = ASC->MakeEffectContext();
			auto SpecHandle = ASC->MakeOutgoingSpec(EffectClass, Level, Context);

			if (!SpecHandle.IsValid()) continue;

			SpecHandle.Data->SetStackCount(Stacks);

			if (RemainingDuration > 0.f)
			{
				SpecHandle.Data->Duration = RemainingDuration;
			}

			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}

		// --- 3. Animation Revert ---
		if (UAnimInstance* AInst = Char->GetMesh()->GetAnimInstance())
		{
			AInst->StopAllMontages(0.f);
			if (FoundState->CurrentMontage.IsValid())
			{
				AInst->Montage_Play(FoundState->CurrentMontage.Get(), 1.f);
				AInst->Montage_SetPosition(FoundState->CurrentMontage.Get(), FoundState->MontagePosition);
			}
		}
	}
}

void UPresageSubsystem::UpdateTimelinePrediction()
{
	CurrentPredictedTimeline.Empty();

	const auto StepSize = 0.1f; // How far to look ahead. TODO: Replace with Flow Time attribute.

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWolfCharacterBase::StaticClass(), AllActors);

	for (auto t = 0.f; t <= FlowTime; t += StepSize)
	{
		for (auto* AttackerActor : AllActors)
		{
			AWolfCharacterBase* Attacker = Cast<AWolfCharacterBase>(AttackerActor);
			if (!Attacker) continue;

			auto TimeToHit = Attacker->GetTimeToNextHitImpact();

			if (FMath::IsNearlyEqual(TimeToHit, t, StepSize * 0.f))
			{
				for (auto* VictimActor : AllActors)
				{
					if (Attacker == VictimActor) continue;
					auto* Victim = Cast<AWolfCharacterBase>(VictimActor);

					if (CheckFutureCollision(Attacker, Victim, t))
					{
						FPresageTimelineEvent NewEvent(Attacker, Victim, t, FGameplayTag::EmptyTag);
						CurrentPredictedTimeline.Add(NewEvent);
					}
				}
			}
		}
	}
}

bool UPresageSubsystem::CheckFutureCollision(AWolfCharacterBase* Attacker, AWolfCharacterBase* Victim, float FutureTime)
{
	auto AttackerTransform = Attacker->GetActorTransform();
	auto VictimTransform = Victim->GetActorTransform();

	float Radius, HalfHeight;
	Victim->GetPresageCollisionDimensions(Radius, HalfHeight);

	FVector AttackLocation = AttackerTransform.GetLocation() + AttackerTransform.GetRotation().GetForwardVector() *
		100.f;
	auto Distance = FVector::Dist(AttackLocation, VictimTransform.GetLocation());

	return Distance < Radius + 50.f;
}
