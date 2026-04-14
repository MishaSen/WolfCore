// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Abilities/BaseCombatAbility.h"

#include "AIController.h"
#include "Abilities/Notifies/AnimNotify_Hit.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/WolfCharacterBase.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_MoveToLocation.h"

UBaseCombatAbility::UBaseCombatAbility()
{
	ActivationBlockedTags.AddTag(FWolfGameplayTags::Get().InputState_Dead);
	CancelAbilitiesWithTag.AddTag(FWolfGameplayTags::Get().InputState_Dead);
}


void UBaseCombatAbility::StartCombatSequence()
{
	CurrentPeriodIndex = 0;
	WOLF_LOG(Log, TEXT("Combat sequence started with %d periods."), AbilitySequence.Num());
	PlayNextPeriod();
}

void UBaseCombatAbility::ExecuteWait(const FCombatPeriod& Period)
{
	/* Consider implementing something like ApplyWaitTags() for wait period behavior. E.g., take more damage it hit.*/
	if (IsValid(Period.Montage)) { ExecuteAnimatedPeriod(Period); }
	else
	{
		auto* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Period.Duration);
		DelayTask->OnFinish.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		DelayTask->ReadyForActivation();
	}
}

void UBaseCombatAbility::ExecuteAnimatedPeriod(const FCombatPeriod& Period)
{
	if (IsValid(Period.Montage)) // Currently does not support non attack period montages.
	{
		auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Period.Montage, 1.f, NAME_None, false,
				1.f, 0.f, false);

		if (Period.Type == EPeriodType::Attack && HitEventTag.IsValid())
		{
			auto* WaitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
				this, HitEventTag, nullptr);
			
			WaitTask->EventReceived.AddDynamic(this, &UBaseCombatAbility::OnEventReceived);

			MontageTask->OnCompleted.AddDynamic(WaitTask, &UAbilityTask_WaitGameplayEvent::EndTask);
			MontageTask->OnInterrupted.AddDynamic(WaitTask, &UAbilityTask_WaitGameplayEvent::EndTask);
			MontageTask->OnCancelled.AddDynamic(WaitTask, &UAbilityTask_WaitGameplayEvent::EndTask);

			WaitTask->ReadyForActivation();
		}

		MontageTask->OnCompleted.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UBaseCombatAbility::K2_EndAbility);
		MontageTask->OnCancelled.AddDynamic(this, &UBaseCombatAbility::K2_EndAbility);
		MontageTask->ReadyForActivation();
	}
	else
	{
		if (Period.Type == EPeriodType::Attack)
		{
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle,
				[this, Period]() { HandleAttackHitEvent(Period); },
				Period.HitDelay, false);
		}

		auto* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Period.Duration);
		DelayTask->OnFinish.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		DelayTask->ReadyForActivation();
	}
}

void UBaseCombatAbility::PlayNextPeriod()
{
	if (!AbilitySequence.IsValidIndex(CurrentPeriodIndex))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	auto& CombatPeriod = AbilitySequence[CurrentPeriodIndex];

	switch (CombatPeriod.Type)
	{
		case EPeriodType::MoveTo: ExecuteMoveTo(CombatPeriod); break;
		case EPeriodType::Wait:	  ExecuteWait(CombatPeriod);   break;
		
		case EPeriodType::Attack:
		case EPeriodType::Windup:
		case EPeriodType::Evasion: ExecuteAnimatedPeriod(CombatPeriod); break;
	}
}

void UBaseCombatAbility::ExecuteMoveTo(FCombatPeriod& Period)
{
	const auto* Target = GetTargetFromBlackboard();
	WOLF_LOG(Log, TEXT("Executing MoveTo for ability %s from character %s and targeting %s"),
		*GetName(), *GetAvatarActorFromActorInfo()->GetName(), *Target->GetName());
	
	const auto* AvatarActor = GetAvatarActorFromActorInfo();
	if (!Target || !AvatarActor) { OnPeriodCompleted(); return; }

	const auto* AvatarCharacter = Cast<AWolfCharacterBase>(AvatarActor);
	if (IsValid(AvatarCharacter))
	{
		const auto* MoveComp = AvatarCharacter->GetCharacterMovement();
		const auto TargetLocation = Target->GetActorLocation();
		const auto CurrentLocation = AvatarActor->GetActorLocation();

		const auto Direction = (TargetLocation - CurrentLocation).GetSafeNormal2D();
		const auto GoalLocation = TargetLocation - Direction * Period.Range;
		// Moves backwards if too close. Need to review down the line if this behavior should be intended.
		const auto MoveDistance = FVector::Dist(CurrentLocation, GoalLocation);

		const auto MaxSpeed = MoveComp->MaxWalkSpeed;
		const auto Acceleration = MoveComp->MaxAcceleration;
		const auto CurrentVelocity = AvatarActor->GetVelocity().Size();

		Period.Duration = CalculateMovementDuration(MoveDistance, MaxSpeed, Acceleration, CurrentVelocity);
		Period.MoveToDestination = GoalLocation;

		WOLF_LOG(Log, TEXT("%s moving to position %.2f. Target's location is at %.2f"),
			*AvatarActor->GetName(), MoveDistance, FVector::Dist(CurrentLocation, TargetLocation));
		
		auto* MoveTask = UAbilityTask_MoveToLocation::MoveToLocation(
			this,
			TEXT("PresageMoveTo"),
			GoalLocation,
			Period.Duration,
			nullptr,
			nullptr);

		MoveTask->OnTargetLocationReached.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		MoveTask->ReadyForActivation();
	}
}

float UBaseCombatAbility::CalculateMovementDuration(float TotalDistance, float MaxVelocity, float Acceleration, float StartVelocity)
{
	if (Acceleration <= 0.f) return StartVelocity > 0.f ? TotalDistance / StartVelocity : 0.f;
	if (StartVelocity >= MaxVelocity) return TotalDistance / StartVelocity;
	
	const auto TimeToReachMax = (MaxVelocity - StartVelocity) / Acceleration;
	const auto DistanceCoveredDuringAcceleration = StartVelocity * TimeToReachMax + 0.5f * Acceleration * FMath::Square(TimeToReachMax);

	if (DistanceCoveredDuringAcceleration >= TotalDistance) // Never reach MaxVelocity
	{
		return (-StartVelocity + FMath::Sqrt(FMath::Square(StartVelocity) + 2 * Acceleration * TotalDistance)) / Acceleration;
	}

	const auto RemainingDistance = TotalDistance - DistanceCoveredDuringAcceleration;
	const auto TimeAtMax = RemainingDistance / MaxVelocity;
	return TimeToReachMax + TimeAtMax;
}

AActor* UBaseCombatAbility::GetTargetFromBlackboard() const
{
	const auto Info = GetActorInfo();
	if (!Info.AvatarActor.IsValid()) return nullptr;

	const auto* Controller = Info.AvatarActor->GetInstigatorController();
	const AAIController* AIC = Cast<AAIController>(Controller);
	if (AIC)
	{
		if (const auto* BB = AIC->GetBlackboardComponent())
		{
			static const FName TargetKey = TEXT("TargetActor"); // Static FName prevents re-hasing the string every frame/call
			return Cast<AActor>(BB->GetValueAsObject(TargetKey));
		}
	}
	return nullptr;
}

void UBaseCombatAbility::OnPeriodCompleted()
{
	CurrentPeriodIndex++;
	PlayNextPeriod();
}

void UBaseCombatAbility::OnEventReceived(FGameplayEventData EventData)
{
	if (AbilitySequence.IsValidIndex(CurrentPeriodIndex)) { HandleAttackHitEvent(AbilitySequence[CurrentPeriodIndex]); }
}

void UBaseCombatAbility::HandleAttackHitEvent(const FCombatPeriod& CurrentAttackPeriod)
{
	// Override in children
}

float UBaseCombatAbility::GetPeriodDuration(const FCombatPeriod& Period)
{
	if (Period.Montage) return Period.Montage->GetPlayLength();
	return Period.Duration;
}

float UBaseCombatAbility::CalculateProjectedImpactTime() const
{
	float TimeAccumulator = 0.f;
	for (const auto& Period : AbilitySequence)
	{
		if (Period.Type != EPeriodType::Attack)
		{
			TimeAccumulator += GetPeriodDuration(Period);
			continue;
		}

		if (!Period.Montage)
		{
			return TimeAccumulator + Period.HitDelay;
		}
		
		for (const auto& NotifyEvent : Period.Montage->Notifies)
		{
			const auto* HitNotify = Cast<UAnimNotify_Hit>(NotifyEvent.Notify);
			if (HitNotify && HitNotify->EventTag == HitEventTag)
			{
				return TimeAccumulator + NotifyEvent.GetTriggerTime();
			}
		}
	}
	return -1.f;
}

bool UBaseCombatAbility::IsInvulnerableAt(float RelativeTime) const
{
	float TimeAccumulator = 0.f;
	for (const auto& Period : AbilitySequence)
	{
		const float PeriodEnd = TimeAccumulator + GetPeriodDuration(Period);
		if (RelativeTime >= TimeAccumulator && RelativeTime < PeriodEnd)
		{
			return Period.Type == EPeriodType::Evasion;
		}
		TimeAccumulator = PeriodEnd;
	}
	return false;
}