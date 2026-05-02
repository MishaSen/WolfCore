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
	/** Handles input press events for ability tags, triggering corresponding abilities on the character. */
	/**
	 * @param Tag The FGameplayTag representing the pressed input action.
	 */
	void AbilityInputTagPressed(const FGameplayTag& Tag);

	/** Handles input release events for ability tags, releasing held abilities or triggering cancel actions. */
	/**
	 * @param Tag The FGameplayTag representing the released input action.
	 */
	void AbilityInputTagReleased(const FGameplayTag& Tag);

	/** Handles continuous input hold events for ability tags while the button is held down. */
	/**
	 * @param Tag The FGameplayTag representing the held input action.
	 */
	void AbilityInputTagHeld(const FGameplayTag& Tag);

	// ============================================================================================================================
	// Public API - Ability Management
	// ============================================================================================================================

	/** Grants startup abilities to this ASC, registering them with the Gameplay Ability System for activation. */
	/**
	 * @param StartupAbilities Array of ability subclasses to grant and activate on this component.
	 * @return TMap of ability classes to their granted spec handles.
	 */
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> AddCharacterAbilities(TArray<TSubclassOf<UGameplayAbility>> StartupAbilities);

	// ============================================================================================================================
	// Public API - Presage Integration
	// ================================================================= ============================================================

	/**Builds an initial presage ability request for temporal prediction and snapshot-based combat resolution. */
	/**
	 * @param Tag The FGameplayTag representing the ability being requested for presage simulation.
	 * @param Targets Array of weak actor references representing potential targets for ability prediction.
	 * @return FPresageAbilityRequest containing the request data for temporal prediction queries.
	 */
	FPresageAbilityRequest BuildInitialPresageRequest(const FGameplayTag& Tag, const TArray<TWeakObjectPtr<AActor>>& Targets);
};