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

	const auto* CombatModeSubsystem = GetWorld()->GetSubsystem<UCombatModeSubsystem>();
	if (!CombatModeSubsystem)
	{
		WOLF_ERROR(TEXT("Failed to get CombatModeSubsystem from world %s"), *GetWorld()->GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (CombatModeSubsystem->GetCombatMode() != ECombatMode::RT)
	{
		WOLF_ERROR(TEXT("RTCombatAbility activated outside of RT mode."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	WOLF_INFO(TEXT("Entering RT ability startup."));
	OnStartupPhase();
}

void URTCombatAbility::EnterActivePhase()
{
	WOLF_INFO(TEXT("Entering RT ability active phase."));
	OnActivePhase();
}

void URTCombatAbility::EnterRecoveryPhase()
{
	WOLF_INFO(TEXT("Entering RT ability recovery phase."));
	OnRecoveryPhase();
}

void URTCombatAbility::EndAbilityPhase()
{
	GetWorld()->GetTimerManager().ClearTimer(StartupTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(ActiveTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URTCombatAbility::OnStartupPhase()
{
	const auto StartupTime = FrameData ? FrameData->StartupTime : 0.1f;
	GetWorld()->GetTimerManager().SetTimer(StartupTimerHandle,
	                                       this,
	                                       &ThisClass::EnterActivePhase,
	                                       StartupTime,
	                                       false);
}

void URTCombatAbility::OnActivePhase()
{
	const auto ActiveTime = FrameData ? FrameData->ActiveTime : 0.1f;
	GetWorld()->GetTimerManager().SetTimer(ActiveTimerHandle,
										   this,
										   &ThisClass::EnterRecoveryPhase,
										   ActiveTime,
										   false);
}

void URTCombatAbility::OnRecoveryPhase()
{
	const auto RecoveryTime = FrameData ? FrameData->RecoveryTime : 0.1f;
	GetWorld()->GetTimerManager().SetTimer(RecoveryTimerHandle,
										   this,
										   &ThisClass::EndAbilityPhase,
										   RecoveryTime,
										   false);
}
