// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/SwitchMode.h"

#include "AbilitySystemComponent.h"
#include "Debug/WolfDebug.h"
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
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		WOLF_ERROR(TEXT("AbilitySystemComponent missing on actor during SwitchMode activation."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const auto* World = ActorInfo->AbilitySystemComponent->GetWorld();
	if (!World)
	{
		WOLF_ERROR(TEXT("Invalid World during SwitchMode activation."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (auto* CombatModeSubsystem = World->GetSubsystem<UCombatModeSubsystem>())
	{
		const auto CurrentMode = CombatModeSubsystem->GetCombatMode();
		const auto NewMode = CurrentMode == ECombatMode::RT ? ECombatMode::TB : ECombatMode::RT;

		CombatModeSubsystem->SetCombatMode(NewMode);

		WOLF_LOG(Log, TEXT("SwitchMode ability triggered: %s -> %s"),
			*UEnum::GetValueAsString(CurrentMode),
			*UEnum::GetValueAsString(NewMode));
	}
	else
	{
		WOLF_ERROR(TEXT("Failed to get CombatModeSubsystem from world %s"), *GetWorld()->GetName());
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}