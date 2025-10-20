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

	if (ASC->HasMatchingGameplayTag(FWolfGameplayTags::Get().InputState_TB))
	{
		ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(FWolfGameplayTags::Get().InputState_TB));
		GEngine->AddOnScreenDebugMessage(
			4,
			3.f,
			FColor::Yellow,
			FString::Printf(TEXT("Switching Mode: TB -> RT"))
		);
	}
	else
	{
		if (!PresageModeGEClass)
		{
			UE_LOG(LogTemp, Error, TEXT("PresageModeGEClass not set on the SwitchMode Ability."))
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}
		const FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		const FActiveGameplayEffectHandle NewHandle = ASC->ApplyGameplayEffectToSelf(
			PresageModeGEClass.GetDefaultObject(),
			1.f,
			ContextHandle
		);
		if (!NewHandle.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to apply GE to self."))
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}
		GEngine->AddOnScreenDebugMessage(
			4,
			3.f,
			FColor::Yellow,
			FString::Printf(TEXT("Switching Mode: RT -> TB"))
		);
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}