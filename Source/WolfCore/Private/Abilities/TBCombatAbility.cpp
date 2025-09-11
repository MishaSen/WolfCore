// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Abilities/TBCombatAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

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
		       TEXT("Current Period: %s | Duration: %.2f | Invulnerability: %s"),
		       *EnumPtr->GetNameStringByValue(static_cast<uint64>(Period.PeriodType)).RightChop(FString("EPeriod::").Len
			       ()),
		       Period.Duration,
		       Period.bIsInvulnerable ? TEXT("true") : TEXT("false"));
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
		MontageTask->OnCompleted.AddDynamic(this, &UTBCombatAbility::OnDelayFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &UTBCombatAbility::K2_EndAbility);
		MontageTask->OnCancelled.AddDynamic(this, &UTBCombatAbility::K2_EndAbility);

		MontageTask->ReadyForActivation();
	}
	else if (Period.Duration > 0.f)
	{
		UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Period.Duration);
		DelayTask->OnFinish.AddDynamic(this, &UTBCombatAbility::OnDelayFinished);

		DelayTask->ReadyForActivation();
	}
	else
	{
		++CurrentPeriodIndex;
		PlayCurrentPeriod(Handle, ActorInfo, ActivationInfo);
	}
}

void UTBCombatAbility::OnDelayFinished()
{
	++CurrentPeriodIndex;
	PlayCurrentPeriod(CachedHandle, CachedActorInfo, CachedActivationInfo);
}
