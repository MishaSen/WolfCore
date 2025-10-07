// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/SwitchMode.h"

#include "Core/WolfPlayerController.h"

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
	if (const auto Controller = ActorInfo->PlayerController.Get())
	{
		if (const auto WolfPC = Cast<AWolfPlayerController>(Controller))
		{
			const EInputContext CurrentContext = WolfPC->CurrentInputContext;
			const bool bIsEnteringTB = (CurrentContext == EInputContext::InCombatRT);
			// If it is RT, we are switching to TB

			WolfPC->HandleTBTransition(bIsEnteringTB);

			GEngine->AddOnScreenDebugMessage(4, 3.f, FColor::Yellow,
			                                 FString::Printf(
				                                 TEXT("Switched to: %s"),
				                                 bIsEnteringTB ? TEXT("Turn-Based") : TEXT("Real-Time")));
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
