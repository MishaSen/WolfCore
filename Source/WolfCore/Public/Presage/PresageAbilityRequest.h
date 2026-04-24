// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/BaseCombatAbility.h"
#include "Abilities/TBCombatAbility.h"

#include "PresageAbilityRequest.generated.h"


/**
 * Struct representing a request to execute a combat ability during presage simulation with temporal prediction data.
 */
USTRUCT(BlueprintType)
struct FPresageAbilityRequest
{
	GENERATED_BODY()

	/** Default constructor creating an empty ability request with no active parameters. */
	FPresageAbilityRequest()
	{
	}

	/** Full constructor initializing all fields for a presage ability request with combat and temporal data. */
	/**
	 * @param InAbilityClass The subclass of UTBCombatAbility to execute during simulation.
	 * @param InASC Pointer to the AbilitySystemComponent owning this request.
	 * @param InInputTag The GameplayTag representing the input action triggering this ability.
	 * @param InRequestedTime The scheduled execution time in seconds for temporal prediction.
	 * @param InAbilitySequence Array of FCombatPeriod entries defining the combat sequence.
	 * @param InTargets Array of weak actor references representing potential targets.
	 */
	explicit  FPresageAbilityRequest(
		const TSubclassOf<UTBCombatAbility>& InAbilityClass,
		const TObjectPtr<UAbilitySystemComponent>& InASC,
		const FGameplayTag& InInputTag,
		float InRequestedTime,
		const TArray<FCombatPeriod>& InAbilitySequence,
		const TArray<TWeakObjectPtr<AActor>>& InTargets
	):
		AbilityClass(InAbilityClass),
		OwnerASC(InASC),
		InputTag(InInputTag),
		ScheduledTime(InRequestedTime),
		AbilitySequence(InAbilitySequence),
		Targets(InTargets)
	{
	}

	/** Subclass of UTBCombatAbility representing the ability type requested for execution during simulation. */
	UPROPERTY()
	TSubclassOf<UTBCombatAbility> AbilityClass;

	/** Retrieves the class default object (CDO) instance of the requested combat ability for configuration access. */
	UTBCombatAbility* GetAbilityCDO() const
	{
		return AbilityClass ? AbilityClass->GetDefaultObject<UTBCombatAbility>() : nullptr;
	}

	// ============================================================================================================================
	// Accessors
	// ============================================================================================================================

	/** Retrieves the owner's Ability System Component (ASC) for Gameplay Ability System interactions. */
	TObjectPtr<UAbilitySystemComponent> GetOwnerASC() const { return OwnerASC; }

	/** Returns the input tag representing the ability action triggering this request. */
	const FGameplayTag& GetInputTag() const { return InputTag; }

	/** Returns the scheduled execution time in seconds for temporal prediction queries. */
	float GetScheduledTime() const { return ScheduledTime; }

	/** Returns the array of combat periods defining the ability sequence for this request. */
	const TArray<FCombatPeriod>& GetAbilitySequence() const { return AbilitySequence; }

	/** Returns the array of weak actor references representing potential targets for prediction. */
	const TArray<TWeakObjectPtr<AActor>>& GetTargets() const { return Targets; }

private:
	/** Strong reference to the owner's Ability System Component (ASC) for Gameplay Ability System interactions. */
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> OwnerASC = nullptr;

	/** GameplayTag representing the input action that triggered this ability request. */
	UPROPERTY()
	FGameplayTag InputTag;

	/** Scheduled execution time in seconds for temporal prediction and snapshot-based resolution. */
	UPROPERTY()
	float ScheduledTime = 0.f;

	/** Array of FCombatPeriod entries defining the complete combat sequence for this ability request. */
	UPROPERTY()
	TArray<FCombatPeriod> AbilitySequence;

	/** Array of weak actor references representing potential targets for ability prediction and targeting. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Targets;
};