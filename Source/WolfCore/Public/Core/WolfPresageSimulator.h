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
	 * @param StepSize
	 */
	static void ExecuteFutureBake(const TArray<TScriptInterface<IWolfCombatant>>& Combatants, float Duration, float StepSize);

	/**
	 * GENERALIZED mode-conditional drain applier (ResourceLoop stage 4). Adds the given drain
	 * GameplayEffect to an ASC when the actor's mode matches DrainActiveInMode, and removes it
	 * otherwise — the exact add/remove structure the (now retired) TB presage drain used, made
	 * generic over the mode so any "drain while in mode X" effect shares one query-check path.
	 * This is the pattern's only live user: the RT Adrenaline drain (URTAdrenalineDrain, active in
	 * InputState.RT) is applied/removed from UCombatModeSubsystem::ApplyModeToActor for the player.
	 *
	 * Handles double-application by querying for an existing active instance of the effect first
	 * (no-op when present state already matches). Removes by source effect class so every instance
	 * is cleared.
	 *
	 * @param ASC              The AbilitySystemComponent to add/remove the drain on.
	 * @param CurrentActorMode The actor's current combat mode being evaluated.
	 * @param DrainActiveInMode The mode the drain should be PRESENT in (effect applied iff this
	 *        equals CurrentActorMode).
	 * @param DrainEffectClass The GameplayEffect class to apply/remove.
	 */
	static void ApplyModeDrainEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode, FGameplayTag DrainActiveInMode, TSubclassOf<UGameplayEffect> DrainEffectClass);

private:
	// Internal helper for simulation setup per combatant
	static void SetupCombatantSimulation(const TScriptInterface<IWolfCombatant>& Combatant, float Duration);

	// Internal helper for simulation baking steps
	static void BakeSimulationStep(const TScriptInterface<IWolfCombatant>& Combatant, float StepSize);

	// Internal helper for simulation cleanup per combatant
	static void CleanupCombatantSimulation(const TScriptInterface<IWolfCombatant>& Combatant);
};