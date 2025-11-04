// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Presage/PresageSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "WolfCore/Public/Presage/ActorState.h"
#include "WolfCore/Public/Presage/PresageAbilityRequest.h"
#include "Components/PrimitiveComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/WolfGameplayTags.h"
#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"
#include "TimerManager.h"

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
	CaptureCharacterStates();
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

void UPresageSubsystem::CaptureCharacterStates()
{
	OriginalCharacterStates.Empty();

	TArray<AActor*> Characters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Characters);
	for (AActor* Actor : Characters)
	{
		ACharacter* Char = Cast<ACharacter>(Actor);
		if (!Char) continue;

		FActorState NewState;
		NewState.ActorRef = Char;
		NewState.Location = Char->GetActorLocation();
		NewState.Rotation = Char->GetActorRotation();
		NewState.Velocity = Char->GetVelocity();

		if (const UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
		{
			NewState.MovementMode = MoveComp->MovementMode;
			NewState.CustomMovementMode = MoveComp->CustomMovementMode;
		}

		if (const UAnimInstance* AInst = Char->GetMesh()->GetAnimInstance())
		{
			if (UAnimMontage* Mon = AInst->GetCurrentActiveMontage())
			{
				NewState.CurrentMontage = Mon;
				NewState.MontagePosition = AInst->Montage_GetPosition(Mon);
			}
		}

		OriginalCharacterStates.Add(NewState);
	}
}

void UPresageSubsystem::RevertCharacterStates()
{
	TArray<AActor*> Characters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Characters);
	for (AActor* Actor : Characters)
	{
		ACharacter* Char = Cast<ACharacter>(Actor);
		if (!Char) continue;

		// Find saved state with matching ActorRef
		const FActorState* FoundState = OriginalCharacterStates.FindByPredicate([Char](const FActorState& State)
		{
			return State.ActorRef == Char;
		});
		if (!FoundState) continue;

		// Send Character back to the saved original position
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
