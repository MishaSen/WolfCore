// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/SwitchMode.h"

#include "AbilitySystemComponent.h"
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
	const bool bIsInRT = ASC->HasMatchingGameplayTag(Tags.InputState_RT);

	if (const FGameplayTag OldStateTag = bIsInRT ? Tags.InputState_RT : Tags.InputState_TB; OldStateTag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(OldStateTag);
	}
	if (const FGameplayTag NewStateTag = bIsInRT ? Tags.InputState_TB : Tags.InputState_RT; NewStateTag.IsValid())
	{
		ASC->AddLooseGameplayTag(NewStateTag);
	}

	FGameplayEventData Payload;
	Payload.EventMagnitude = bIsInRT ? 1.f : 0.f;
	Payload.Instigator = ActorInfo->OwnerActor.Get();
	Payload.Target = ActorInfo->OwnerActor.Get();
	ASC->HandleGameplayEvent(Tags.Event_ModeSwitch, &Payload);

	GEngine->AddOnScreenDebugMessage(
		4,
		3.f,
		FColor::Yellow,
		bIsInRT ? TEXT("Switched to Turn-Based") : TEXT("Switched to Real-Time")
	);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
