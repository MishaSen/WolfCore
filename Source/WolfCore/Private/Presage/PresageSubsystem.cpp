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
		if (const FPresageAbilityRequest& Request = AbilityQueue[i]; Request.GetScheduledTime() <= CurrentTime)
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

void UPresageSubsystem::RefreshParticipants()
{
	TBParticipants.Empty();
	RTParticipants.Empty();

	TArray<AActor*> AllCharacters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWolfCharacterBase::StaticClass(), AllCharacters);

	const auto& Tags = FWolfGameplayTags::Get();

	for (auto* Actor : AllCharacters)
	{
		auto* Character = Cast<AWolfCharacterBase>(Actor);
		if (!Character) continue;

		auto* ASC = Character->GetAbilitySystemComponent();
		if (!ASC) continue;

		if (ASC->HasMatchingGameplayTag(Tags.InputState_TB))
		{
			TBParticipants.Add(Character);
		}
		else if (ASC->HasMatchingGameplayTag(Tags.InputState_RT))
		{
			RTParticipants.Add(Character);
		}
	}
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

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsWithInterface(GetWorld(), USnapshot::StaticClass(), Actors);

	for (auto* Actor : Actors)
	{
		FActorSnapshot NewSnapshot;

		Execute_CreateSnapshot(Actor, NewSnapshot);
		OriginalCharacterStates.Add(NewSnapshot);
	}
}

void UPresageSubsystem::RevertCharacterStates()
{
	for (const auto& Snapshot : OriginalCharacterStates)
	{
		if (!Snapshot.ActorRef) return;
		Execute_RestoreSnapshot(Snapshot.ActorRef, Snapshot);
	}
}

void UPresageSubsystem::UpdateTimelinePrediction()
{
	CurrentPredictedTimeline.Empty();
	RefreshParticipants();

	const auto& Tags = FWolfGameplayTags::Get();
	TArray<FPresageTimelineEvent> PotentialEvents;

	for (auto& RTAttacker : RTParticipants)
	{
		auto* RTCharacter = RTAttacker.Get();
		if (!RTCharacter) continue;

		auto ImpactTime = RTCharacter->GetTimeToNextHitImpact();
		if (ImpactTime < 0.f || ImpactTime > FlowTime) continue;

		for (auto& TBAttacker : TBParticipants)
		{
			auto* TBCharacter = TBAttacker.Get();
			if (!TBCharacter) continue;

			if (CheckFutureCollision(RTCharacter, TBCharacter, ImpactTime))
			{
				PotentialEvents.Add(FPresageTimelineEvent(RTCharacter, TBCharacter, ImpactTime, Tags.Result_Hit));
			}
		}
	}

	for (const auto& Request : AbilityQueue)
	{
		auto* AbilityCDO = Request.GetAbilityCDO();
		if (!AbilityCDO) continue;

		auto* TBAttacker = Cast<AWolfCharacterBase>(Request.GetOwnerASC()->GetAvatarActor());
		if (!TBAttacker) continue;
		
		auto AbilitySequenceImpactTime = CalculateImpactFromSequence(Request.GetAbilitySequence());
		if (AbilitySequenceImpactTime < 0.f) continue;

		auto PresageSequenceImpactTime = AbilitySequenceImpactTime + Request.GetScheduledTime(); 

		for (auto& TargetActor : Request.GetTargets())
		{
			auto* Target = Cast<AWolfCharacterBase>(TargetActor);
			if (!Target) continue;

			PotentialEvents.Add(FPresageTimelineEvent(TBAttacker, Target, PresageSequenceImpactTime, Tags.Result_Hit));
		}
	}

	PotentialEvents.Sort([](const FPresageTimelineEvent& A, const FPresageTimelineEvent& B)
	{
		return A.Time < B.Time;
	});

	TSet<AActor*> InterruptedActors;

	for (auto& Event : PotentialEvents)
	{
		if (InterruptedActors.Contains(Event.Attacker)) continue;

		auto* Victim = Cast<AWolfCharacterBase>(Event.Victim);

		bool bIsInvulnerable = false;
		if (Victim)
		{
			bIsInvulnerable = Victim->IsInvulnerableAt(Event.Time);
		}

		if (bIsInvulnerable)
		{
			Event.ResultTag = Tags.Result_Dodge;
		}
		else
		{
			Event.ResultTag = Tags.Result_Hit;
			InterruptedActors.Add(Event.Victim);
		}

		CurrentPredictedTimeline.Add(Event);
	}
}

bool UPresageSubsystem::CheckFutureCollision(AWolfCharacterBase* Attacker, AWolfCharacterBase* Victim, float FutureTime)
{
	const auto AttackerTransform = Attacker->GetProjectedTransform(FutureTime);

	float Radius, HalfHeight;
	Victim->GetPresageCollisionDimensions(Radius, HalfHeight);

	const FVector AttackLocation = AttackerTransform.GetLocation() + AttackerTransform.GetRotation().GetForwardVector()
		* 100.f;
	const float Distance = FVector::Dist(AttackLocation, Victim->GetActorLocation());

	return Distance < Radius + 50.f;
}

float UPresageSubsystem::CalculateImpactFromSequence(const TArray<FPeriod>& Sequence) const
{
	auto TimeAccumulator = 0.f;
	for (const auto& Period : Sequence)
	{
		if (Period.PeriodType == EPeriod::Attack) return TimeAccumulator;
		
		TimeAccumulator += Period.Duration;
	}
	return -1.f;
}
