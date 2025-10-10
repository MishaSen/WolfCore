// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/SwitchMode.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/WolfAbilitySystemComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Engine/Engine.h"

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
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const FWolfGameplayTags& Tags = FWolfGameplayTags::Get();
	const bool bIsCurrentlyInRT = ASC->HasMatchingGameplayTag(Tags.InputState_RT);
	const ECombatMode TargetMode = bIsCurrentlyInRT ? TB : RT;
	const FGameplayTag TargetTag = TargetMode == TB ? Tags.InputState_TB : Tags.InputState_RT;

	FGameplayEventData Payload;
	Payload.TargetTags.AddTag(TargetTag);
	Payload.Instigator = ActorInfo->OwnerActor.Get();
	Payload.Target = ActorInfo->OwnerActor.Get();
	ASC->HandleGameplayEvent(Tags.Event_ModeSwitch, &Payload);

	GEngine->AddOnScreenDebugMessage(
		4,
		3.f,
		FColor::Yellow,
		FString::Printf(TEXT("Switching Mode to %s"), *TargetTag.ToString())
	);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
