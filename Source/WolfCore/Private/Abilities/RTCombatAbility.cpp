// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/RTCombatAbility.h"

#include "Abilities/AbilityFrameData.h"
#include "Debug/WolfDebug.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Systems/CombatModeSubsystem.h"

void URTCombatAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo,
                                       const FGameplayAbilityActivationInfo ActivationInfo,
                                       const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CachedWorld = GetWorld();
	if (const auto* CombatModeSubsystem = CachedWorld->GetSubsystem<UCombatModeSubsystem>();
		!CachedWorld || !CombatModeSubsystem || CombatModeSubsystem->GetCombatMode() != ECombatMode::RT)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (AttackMontage)
	{
		auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			AttackMontage,
			1.f,
			NAME_None,
			false,
			1.f,
			0.f,
			false
		);

		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::EndPhase);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::EndPhase);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::EndPhase);

		MontageTask->ReadyForActivation();
		StartupPhase();
		WOLF_INFO(TEXT("RTCombatAbility: Started montage %s"), *AttackMontage.GetName());
	}
	else
	{
		WOLF_WARN(TEXT("No AttackMontage set, skipping playback."));
		StartupPhase();
	}
}

void URTCombatAbility::OnNotifyReceived(FName NotifyName)
{
	if (NotifyName == "Notify_StartupEnd") ActivePhase();
	else if (NotifyName == "Notify_ActiveEnd") RecoveryPhase();
	else if (NotifyName == "Notify_RecoveryEnd") EndPhase();
}

void URTCombatAbility::StartupPhase()
{
}

void URTCombatAbility::ActivePhase()
{
}

void URTCombatAbility::RecoveryPhase()
{
}

void URTCombatAbility::EndPhase()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URTCombatAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle,
                                     const FGameplayAbilityActorInfo* ActorInfo,
                                     const FGameplayAbilityActivationInfo ActivationInfo,
                                     bool bReplicateCancelAbility)
{
	WOLF_INFO(TEXT("Ability cancelled early."));
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
