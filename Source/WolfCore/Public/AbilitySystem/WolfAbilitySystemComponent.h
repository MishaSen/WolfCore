// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Presage/PresageAbilityRequest.h"
#include "WolfAbilitySystemComponent.generated.h"

/**
 * Extended Ability System Component providing input handling, ability management, and presage request building.
 */
UCLASS()
class WOLFCORE_API UWolfAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Public API - Input Handling
	// ============================================================================================================================

public:
	void AbilityInputTagPressed(const FGameplayTag& Tag);
	void AbilityInputTagReleased(const FGameplayTag& Tag);
	void AbilityInputTagHeld(const FGameplayTag& Tag);

	// ============================================================================================================================
	// Public API - Ability Management
	// ============================================================================================================================

	void AddCharacterAbilities(TArray<TSubclassOf<UGameplayAbility>> StartupAbilities);

	// ============================================================================================================================
	// Public API - Presage Integration
	// ============================================================================================================================

	FPresageAbilityRequest BuildInitialPresageRequest(const FGameplayTag& Tag, const TArray<TWeakObjectPtr<AActor>>& Targets);
};