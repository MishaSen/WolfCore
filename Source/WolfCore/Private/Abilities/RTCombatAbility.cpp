// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/RTCombatAbility.h"

#include "Abilities/AbilityFrameData.h"
#include "Debug/WolfDebug.h"
#include "TimerManager.h"
#include "Systems/CombatModeSubsystem.h"

void URTCombatAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo,
                                       const FGameplayAbilityActivationInfo ActivationInfo,
                                       const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CachedWorld = GetWorld();
	if (const auto* CombatModeSubsystem = GetWorld()->GetSubsystem<UCombatModeSubsystem>();
		!CachedWorld || !CombatModeSubsystem || CombatModeSubsystem->GetCombatMode() != ECombatMode::RT)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WOLF_INFO(TEXT("Entering RT ability startup."));
	StartupPhase();
}

void URTCombatAbility::ScheduleNextPhase(FTimerHandle& Handle, void(URTCombatAbility::*NextPhase)(),
	float PhaseDuration)
{
	if (!CachedWorld) return;
	
	CachedWorld->GetTimerManager().SetTimer(
		Handle,
		this,
		NextPhase,
		PhaseDuration,
		false
	);
}

void URTCombatAbility::StartupPhase()
{
	ScheduleNextPhase(StartupTimerHandle, &ThisClass::ActivePhase, FrameData->StartupTime);
}

void URTCombatAbility::ActivePhase()
{
	ScheduleNextPhase(ActiveTimerHandle, &ThisClass::RecoveryPhase, FrameData->ActiveTime);
}

void URTCombatAbility::RecoveryPhase()
{
	ScheduleNextPhase(RecoveryTimerHandle, &ThisClass::EndPhase, FrameData->RecoveryTime);
}

void URTCombatAbility::ClearTimers()
{
	if (!CachedWorld) return;
	
	CachedWorld->GetTimerManager().ClearTimer(StartupTimerHandle);
	CachedWorld->GetTimerManager().ClearTimer(ActiveTimerHandle);
	CachedWorld->GetTimerManager().ClearTimer(RecoveryTimerHandle);
}

void URTCombatAbility::EndPhase()
{
	ClearTimers();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URTCombatAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateCancelAbility)
{
	WOLF_INFO(TEXT("Ability cancelled early."));
	ClearTimers();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}