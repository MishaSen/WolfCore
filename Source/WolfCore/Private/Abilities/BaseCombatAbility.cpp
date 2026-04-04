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

void UBaseCombatAbility::PlayNextPeriod()
{
	if (!AbilitySequence.IsValidIndex(CurrentPeriodIndex))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const auto& CombatPeriod = AbilitySequence[CurrentPeriodIndex];

	switch (CombatPeriod.Type)
	{
		case EPeriodType::MoveTo: ExecuteMoveTo(CombatPeriod); break;
		case EPeriodType::Rotate: ExecuteRotate(CombatPeriod); break;
		case EPeriodType::Wait:	  ExecuteWait(CombatPeriod);   break;
		
		case EPeriodType::Attack:
		case EPeriodType::Windup:
		case EPeriodType::Evasion: ExecuteAnimatedPeriod(CombatPeriod); break;
	}

	// Set up Event Listener if Attack
	if (CombatPeriod.Type == EPeriodType::Attack)
	{
		if (HitEventTag.IsValid())
		{
			auto* WaitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag);
			WaitTask->EventReceived.AddDynamic(this, &UBaseCombatAbility::OnEventReceived);
			WaitTask->ReadyForActivation();
		}
	}

	if (CombatPeriod.Montage)
	{
		auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, CombatPeriod.Montage, 1.f, NAME_None, false,
			1.f, 0.f, false);

		MontageTask->OnCompleted.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UBaseCombatAbility::K2_EndAbility);
		MontageTask->OnCancelled.AddDynamic(this, &UBaseCombatAbility::K2_EndAbility);

		MontageTask->ReadyForActivation();
	}
	else // If the attack period has no montage (e.g., prototyping), play hit event based on timer instead of notify 
	{
		const auto Duration = GetPeriodDuration(CombatPeriod);

		if (CombatPeriod.Type == EPeriodType::Attack)
		{
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UBaseCombatAbility::HandleAttackHitEvent,
			                                       CombatPeriod.HitDelay, false);
		}

		auto* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
		DelayTask->OnFinish.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		DelayTask->ReadyForActivation();
	}
}

void UBaseCombatAbility::ExecuteMoveTo(FCombatPeriod& Period)
{
	const auto* Target = GetTargetFromBlackboard();
	if (!Target) { OnPeriodCompleted(); return; }

	const auto* AvatarActor = GetAvatarActorFromActorInfo();
	const auto* Character = Cast<AWolfCharacterBase>(AvatarActor);

	if (Character)
	{
		const auto* MoveComp = Character->GetCharacterMovement();
		auto Distance = FVector::Dist(AvatarActor->GetActorLocation(), Target->GetActorLocation());
		auto Speed =  MoveComp->MaxWalkSpeed;

		auto* MoveTask = UAbilityTask_MoveToLocation::MoveToLocation(this, TEXT("PresageMoveTo"),
			Target->GetActorLocation(), Period.Duration, nullptr, nullptr);

		MoveTask->OnTargetLocationReached.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		MoveTask->ReadyForActivation();
	}
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
	HandleAttackHitEvent();
}

void UBaseCombatAbility::HandleAttackHitEvent()
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

int32 UBaseCombatAbility::SetCurrentPeriodIndex(int32 NewIndex)
{
	CurrentPeriodIndex = NewIndex;
	return CurrentPeriodIndex;
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
