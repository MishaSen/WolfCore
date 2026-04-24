// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCombatAbility.h"
#include "SwitchMode.generated.h"

/**
 * Ability that switches combat mode and applies Presage-related gameplay effects.
 */
UCLASS()
class WOLFCORE_API USwitchMode : public UBaseCombatAbility
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	USwitchMode();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	// ============================================================================================================================
	// Presage Configuration
	// ============================================================================================================================

	UPROPERTY(EditDefaultsOnly, Category = "Config|Presage")
	TSubclassOf<UGameplayEffect> PresageModeGEClass;
};