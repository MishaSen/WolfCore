// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Abilities/TBCombatAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "DrawDebugHelpers.h"
#include "Abilities/Notifies/AnimNotify_Hit.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Debug/WolfDebug.h"

void UTBCombatAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo,
                                       const FGameplayAbilityActivationInfo ActivationInfo,
                                       const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (AbilitySequence.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No periods defined for TBCombatAbility"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (CachedProjectedHitTime < 0.f)
	{
		RefreshCachedData();
	}

	// Cache context for use in OnDelayFinished()
	CachedHandle = Handle;
	CachedActorInfo = ActorInfo;
	CachedActivationInfo = ActivationInfo;

	CurrentPeriodIndex = 0;
	PlayCurrentPeriod(Handle, ActorInfo, ActivationInfo);
}

void UTBCombatAbility::PlayCurrentPeriod(FGameplayAbilitySpecHandle Handle,
                                         const FGameplayAbilityActorInfo* ActorInfo,
                                         const FGameplayAbilityActivationInfo& ActivationInfo)
{
	if (CurrentPeriodIndex >= AbilitySequence.Num())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const FPeriod& Period = AbilitySequence[CurrentPeriodIndex];

	if (const UEnum* EnumPtr = StaticEnum<EPeriod>()) //
	{
		UE_LOG(LogTemp, Log,
		       TEXT("Current Period: %s | Duration: %.2f"),
		       *EnumPtr->GetNameStringByValue(static_cast<uint64>(Period.PeriodType)).RightChop(FString("EPeriod::").Len
			       ()),
		       Period.Duration
		);
	}

	if (Period.Montage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				NAME_None,
				Period.Montage,
				1.f
			);
		MontageTask->OnCompleted.AddDynamic(this, &UTBCombatAbility::OnPeriodFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &UTBCombatAbility::K2_EndAbility);
		MontageTask->OnCancelled.AddDynamic(this, &UTBCombatAbility::K2_EndAbility);

		MontageTask->ReadyForActivation();

		/*
		 * Listen for the event sent from AnimNotify_Hit
		 * Define the tag in project settings: Gameplay Tags -> Gameplay Abilities -> Event -> Attack
		 */
		if (Period.PeriodType == EPeriod::Attack)
		{
			const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag("Event.Ability.Attack");
			UAbilityTask_WaitGameplayEvent* WaitTask =
				UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag);

			WaitTask->EventReceived.AddDynamic(this, &UTBCombatAbility::HandleGameplayEventHit);
			WaitTask->ReadyForActivation();
		}
	}
	else
	{
		if (Period.Duration > 0.f)
		{
			UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Period.Duration);
			DelayTask->OnFinish.AddDynamic(this, &UTBCombatAbility::OnPeriodFinished);

			DelayTask->ReadyForActivation();
		}
		else
		{
			OnPeriodFinished();
		}
	}
}

float UTBCombatAbility::GetTruePeriodDuration(const FPeriod& Period)
{
	if (Period.bUseMontageLength && Period.Montage)
	{
		return Period.Montage->GetPlayLength();
	}

	return Period.Duration;
}

void UTBCombatAbility::HandleGameplayEventHit_Implementation(FGameplayEventData Payload)
{
	UE_LOG(LogTemp, Warning, TEXT("TB ability hit detected."))
}

void UTBCombatAbility::RefreshCachedData()
{
	CachedProjectedHitTime = CalculateProjectedAttackTime();
}

void UTBCombatAbility::PostInitProperties()
{
	Super::PostInitProperties();

	RefreshCachedData();
}

float UTBCombatAbility::CalculateProjectedAttackTime() const
{
	if (!ImpactTrackingTag.IsValid())
	{
		WOLF_WARN(TEXT("No impact tracking tag set on BaseTBCombatAbility Blueprint"));
		return -1.f;
	}
	
	auto TimeAccumulator = 0.0f;
	for (const auto& Period : AbilitySequence)
	{
		if (Period.PeriodType != EPeriod::Attack)
		{
			TimeAccumulator += GetTruePeriodDuration(Period);
			continue; // Add total period time and go to the next period.
		}

		// Attack periods must have montage
		for (const auto& NotifyEvent : Period.Montage->Notifies)
		{
			if (const auto* HitNotify = Cast<UAnimNotify_Hit>(NotifyEvent.Notify);
				HitNotify->EventTag == ImpactTrackingTag) // Currently assumes one notify per attack montage
			{
				return TimeAccumulator + NotifyEvent.GetTriggerTime();
			}
		}

		return TimeAccumulator + Period.HitDelay;
	}

	WOLF_WARN(TEXT("Ability Sequence has no attack period"));
	return -1.f;
}

bool UTBCombatAbility::IsInvulnerableAt(float RelativeTime) const
{
	auto CurrentTime = 0.0f;
	for (const auto& Period : AbilitySequence)
	{
		const auto PeriodEnd = CurrentTime + GetTruePeriodDuration(Period);

		if (RelativeTime >= CurrentTime && RelativeTime < PeriodEnd)
		{
			return Period.PeriodType == EPeriod::Evasion;
		}
		CurrentTime = PeriodEnd;
	}

	return false;
}

void UTBCombatAbility::OnPeriodFinished()
{
	++CurrentPeriodIndex;
	PlayCurrentPeriod(CachedHandle, CachedActorInfo, CachedActivationInfo);
}
