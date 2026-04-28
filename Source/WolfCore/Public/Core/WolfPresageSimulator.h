// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGameplayEffect;
class IWolfCombatant;
class UAbilitySystemComponent;
class UWolfPresageComponent;

/**
 * Static utility class for presage simulation orchestration.
 * Provides pure state management for temporal prediction without requiring subsystem knowledge.
 * 
 * This utility operates entirely through the IWolfCombatant interface,
 * maintaining decoupling between the Subsystem and concrete character implementations.
 */
struct WOLFCORE_API FWolfPresageSimulator
{
	/**
	 * Executes future state baking for all tracked combatants over a specified duration.
	 * 
	 * This method encapsulates the entire simulation pipeline:
	 * 1. Clear prediction buffers and setup simulation transforms
	 * 2. Sync period progress from active abilities
	 * 3. Run simulation steps (bake)
	 * 4. Cleanup simulation state
	 * 
	 * @param Combatants Array of combatant interfaces to simulate
	 * @param Duration Total simulation duration in seconds
	 */
	static void ExecuteFutureBake(const TArray<TScriptInterface<IWolfCombatant>>& Combatants, float Duration);

	/**
	 * Applies presage drain effect to an actor based on their current combat mode state.
	 * 
	 * This helper manages the GameplayEffect lifecycle for turn-based mode transitions,
	 * adding or removing the drain effect as needed without requiring subsystem knowledge.
	 * 
	 * @param ASC Pointer to the AbilitySystemComponent
	 * @param CurrentActorMode The combat mode being evaluated
	 * @param PresageEffectClass The class of GameplayEffect to apply/remove
	 */
	static void ApplyPresageDrainEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode, TSubclassOf<UGameplayEffect> PresageEffectClass);

private:
	// Internal helper for simulation setup per combatant
	static void SetupCombatantSimulation(const TScriptInterface<IWolfCombatant>& Combatant, float Duration);

	// Internal helper for simulation baking steps
	static void BakeSimulationStep(const TScriptInterface<IWolfCombatant>& Combatant, float StepSize);

	// Internal helper for simulation cleanup per combatant
	static void CleanupCombatantSimulation(const TScriptInterface<IWolfCombatant>& Combatant);
};