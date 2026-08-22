// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Singleton providing access to all native gameplay tags used by the Wolf framework.
 * Tags are organized by category: Input, Input State, Attributes, Effects, Events, Presage Results, Data, and Status.
 */
struct FWolfGameplayTags
{
	// ============================================================================================================================
	// Static Access
	// ============================================================================================================================

	/** Retrieves the singleton instance of the gameplay tags structure for global tag access across the framework. */
	static const FWolfGameplayTags& Get() { return GameplayTags; }

	/** Registers a native gameplay tag with the engine's tag registry using the provided name and description. */
	/**
	 * @param Tag Reference to the FGameplayTag being initialized or modified.
	 * @param TagName The FName representing the raw tag identifier.
	 * @param Description FString providing human-readable documentation for this gameplay tag.
	 */
	static void AddNativeTag(FGameplayTag& Tag, const FName& TagName, const FString& Description);

	/** Initializes all native gameplay tags by calling AddNativeTag for each tag in the framework. Called once at startup. */
	static void InitializeNativeGameplayTags();

	// ============================================================================================================================
	// Input Tags
	// ============================================================================================================================

	/** GameplayTag representing input binding for ability slot 1 (primary melee attack). */
	FGameplayTag Input_Ability1;

	/** GameplayTag representing input binding for ability slot 2 (secondary melee attack). */
	FGameplayTag Input_Ability2;

	/** GameplayTag representing input binding for ability slot 3 (heavy attack variant). */
	FGameplayTag Input_Ability3;

	/** GameplayTag representing input binding for ability slot 4 (special ability activation). */
	FGameplayTag Input_Ability4;

	/** GameplayTag representing the jump input action for character movement. */
	FGameplayTag Input_Jump;

	/** GameplayTag representing the primary attack input for standard combat actions. */
	FGameplayTag Input_Primary;

	/** GameplayTag representing the secondary attack input for combo or variant attacks. */
	FGameplayTag Input_Secondary;

	/** GameplayTag representing the switch mode input for transitioning between combat states. */
	FGameplayTag Input_Switch;

	// ============================================================================================================================
	// Input State Tags
	// ============================================================================================================================

	/** GameplayTag representing the general input state category for tracking player control modes. */
	FGameplayTag InputState;

	/** GameplayTag representing the real-time combat mode input state. */
	FGameplayTag InputState_RT;

	/** GameplayTag representing the turn-based combat mode input state. */
	FGameplayTag InputState_TB;

	/** GameplayTag representing the out-of-context input state for non-combat scenarios. */
	FGameplayTag InputState_OOC;

	/** GameplayTag representing the dead input state where player control is disabled. */
	FGameplayTag InputState_Dead;

	/** GameplayTag representing the invulnerable input state where player actions are blocked. */
	FGameplayTag InputState_Invulnerable;

	// ============================================================================================================================
	// Attributes
	// ============================================================================================================================

	/** GameplayTag identifying the Health attribute for vitality management and combat calculations. */
	FGameplayTag Attribute_Health;

	/** GameplayTag identifying the MaxHealth attribute for upper bound vitality definition. */
	FGameplayTag Attribute_MaxHealth;

	/** GameplayTag identifying the FlowGauge attribute for combat flow resource tracking. */
	FGameplayTag Attribute_FlowGauge;

	/** GameplayTag identifying the Adrenaline attribute for burst ability resource management. */
	FGameplayTag Attribute_Adrenaline;

	// ============================================================================================================================
	// Effects
	// ============================================================================================================================

	/** GameplayTag representing the combat effects category for gameplay effect filtering and application. */
	FGameplayTag Effect_Combat;

	// ============================================================================================================================
	// Event Tags
	// ============================================================================================================================

	/** GameplayTag representing the mode switch ready event for state transition notifications. */
	FGameplayTag Event_ModeSwitchReady;

	/** GameplayTag representing the ability attack event for hit detection and damage resolution triggers. */
	FGameplayTag Event_Ability_Attack;

	// ============================================================================================================================
	// Presage Result
	// ============================================================================================================================

	/** GameplayTag representing the hit result from presage prediction confirming successful impact. */
	FGameplayTag Result_Hit;

	/** GameplayTag representing the dodge result from presage prediction indicating evasion success. */
	FGameplayTag Result_Dodge;

	// ============================================================================================================================
	// Data
	// ============================================================================================================================

	/** GameplayTag representing the amount data category for numerical value tracking and scaling. */
	FGameplayTag Data_Amount;

	// ============================================================================================================================
	// Status
	// ============================================================================================================================

	/** GameplayTag representing the link status for character connection and synchronization state. */
	FGameplayTag Status_Link;

	/** GameplayTag representing a target being airborne — the reaction window for
	  * airborne-extension conditional hit effects (AssistCanary). Granted by a launch ability's
	  * GE_Launched, checked by any ability's ConditionalHitEffects. */
	FGameplayTag Status_Airborne;

	private:
	/** Static singleton instance storing all initialized gameplay tags for framework-wide access. */
	static FWolfGameplayTags GameplayTags;
};