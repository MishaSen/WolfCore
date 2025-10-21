// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/SwitchMode.h"

#include "AbilitySystemComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
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
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		WOLF_ERROR(TEXT("AbilitySystemComponent missing on actor during SwitchMode activation."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const bool bIsCurrentlyTB = ASC->HasMatchingGameplayTag(FWolfGameplayTags::Get().InputState_TB);
	
	if (bIsCurrentlyTB)
	{
		SwitchToRT(ASC);
	}
	else
	{
		SwitchToTB(ASC, Handle, ActorInfo, ActivationInfo);
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void USwitchMode::SwitchToRT(UAbilitySystemComponent* ASC)
{
	ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(FWolfGameplayTags::Get().InputState_TB));
	WOLF_LOG(Log, TEXT("Switching Mode: TB -> RT"));
}

void USwitchMode::SwitchToTB(UAbilitySystemComponent* ASC,
                             const FGameplayAbilitySpecHandle Handle,
                             const FGameplayAbilityActorInfo* ActorInfo,
                             const FGameplayAbilityActivationInfo& ActivationInfo)
{
	if (!PresageModeGEClass)
	{
		WOLF_ERROR(TEXT("PresageModeGEClass not set on SwitchMode ability."));
		return;
	}

	const FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	const FActiveGameplayEffectHandle GEHandle = ASC->ApplyGameplayEffectToSelf(
		PresageModeGEClass.GetDefaultObject(),
		 1.f,
		ContextHandle
	);
	
	if (!GEHandle.IsValid())
	{
		WOLF_ERROR(TEXT("Failed to apply PresageMode GE to self"));
		return;
	}

	WOLF_LOG(Log, TEXT("Switching Mode: RT -> TB"));
}