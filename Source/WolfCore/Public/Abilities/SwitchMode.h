// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCombatAbility.h"
#include "Abilities/GameplayAbility.h"
#include "SwitchMode.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API USwitchMode : public UBaseCombatAbility
{
	GENERATED_BODY()

public:
	USwitchMode();
	UPROPERTY(EditDefaultsOnly, Category = "Config|Presage")
	TSubclassOf<UGameplayEffect> PresageModeGEClass;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
};
