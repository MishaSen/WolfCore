// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"

#include "IWolfCombatant.generated.h"

class UAbilitySystemComponent;
class UWolfPresageComponent;

/**
 * Interface for any actor that can participate in combat mode management.
 * Provides a decoupled abstraction between UCombatModeSubsystem and concrete combatants.
 */
UINTERFACE(BlueprintType)
class WOLFCORE_API UWolfCombatant : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface class implementing IWolfCombatant for combat participation.
 * 
 * This interface allows the CombatModeSubsystem to interact with any actor type
 * (characters, AI, vehicles) without requiring direct knowledge of concrete classes.
 */
class WOLFCORE_API IWolfCombatant
{
	GENERATED_BODY()

public:
	/// @brief Checks if this combatant is currently killable (not invulnerable, not dead).
	/// @return True if the combatant can receive fatal damage; false otherwise.
	virtual bool IsKillable() const = 0;

	/// @brief Retrieves the current combat mode of this combatant.
	/// @return FGameplayTag representing the active combat mode (RT, TB, OOC).
	virtual FGameplayTag GetCurrentCombatMode() const = 0;

	/// @brief Provides access to the AbilitySystemComponent for GAS interactions.
	/// @return Pointer to the UAbilitySystemComponent, or nullptr if unavailable.
	virtual UAbilitySystemComponent* GetASC() const = 0;

	/// @brief Called when this combatant has been selected for death by the subsystem.
	/// @note The concrete actor decides how to execute death (animations, particles, etc).
	virtual void OnTriggerDeath() = 0;

	/// @brief Handles notification when the global combat mode changes.
	/// @param NewMode The FGameplayTag representing the new combat mode being entered.
	UFUNCTION(BlueprintNativeEvent, Category = "Combat")
	void OnCombatModeChanged(FGameplayTag NewMode);

	// ============================================================================================================================
	// Presage Simulation Accessors
	// ============================================================================================================================

	/// @brief Provides access to the presage component for temporal prediction and simulation.
	/// @return Pointer to the UWolfPresageComponent, or nullptr if unavailable.
	virtual UWolfPresageComponent* GetPresageComponent() const = 0;

	/// @brief Retrieves the current ability progress (period time) for simulation sync.
	/// @return Float representing the active ability's period progress in seconds.
	virtual float GetActiveAbilityProgress() const = 0;
};
