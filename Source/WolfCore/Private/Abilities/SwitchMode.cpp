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
	const FGameplayTag PresageModeTag = Tags.InputState_TB;

	FActiveGameplayEffectHandle PresageGEHandle;
	bool bPresageActive = false;

	FGameplayTagContainer PresageTags;
	PresageTags.AddTag(PresageModeTag);

	for (auto ActiveGEHandles = ASC->GetActiveGameplayEffects().GetAllActiveEffectHandles();
	     const auto& GEHandle : ActiveGEHandles)
	{
		if (const auto* ActiveGE = ASC->GetActiveGameplayEffect(GEHandle))
		{
			FGameplayTagContainer GrantedTags;
			ActiveGE->Spec.GetAllGrantedTags(GrantedTags);
			if (GrantedTags.HasTag(PresageModeTag))
			{
				PresageGEHandle = ActiveGE->Handle;
				bPresageActive = true;
				break;
			}
		}
	}

	if (bPresageActive)
	{
		ASC->RemoveActiveGameplayEffect(PresageGEHandle);
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
		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		FActiveGameplayEffectHandle NewHandle = ASC->ApplyGameplayEffectToSelf(
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