// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/SwitchMode.h"

#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Systems/CombatModeSubsystem.h"

USwitchMode::USwitchMode()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void USwitchMode::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                  const FGameplayAbilityActorInfo* ActorInfo,
                                  const FGameplayAbilityActivationInfo ActivationInfo,
                                  const FGameplayEventData* TriggerEventData)
{
	auto* CombatModeSubsystem = GetWorld()->GetSubsystem<UCombatModeSubsystem>();
	if (!CombatModeSubsystem) return;

	CombatModeSubsystem->SwitchCombatMode();
}