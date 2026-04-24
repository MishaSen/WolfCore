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

	static const FWolfGameplayTags& Get() { return GameplayTags; }
	static void AddNativeTag(FGameplayTag& Tag, const FName& TagName, const FString& Description);
	static void InitializeNativeGameplayTags();

	// ============================================================================================================================
	// Input Tags
	// ============================================================================================================================

	FGameplayTag Input_Ability1;
	FGameplayTag Input_Ability2;
	FGameplayTag Input_Ability3;
	FGameplayTag Input_Ability4;
	FGameplayTag Input_Jump;
	FGameplayTag Input_Primary;
	FGameplayTag Input_Secondary;
	FGameplayTag Input_Switch;

	// ============================================================================================================================
	// Input State Tags
	// ============================================================================================================================

	FGameplayTag InputState;
	FGameplayTag InputState_RT;
	FGameplayTag InputState_TB;
	FGameplayTag InputState_OOC;
	FGameplayTag InputState_Dead;

	// ============================================================================================================================
	// Attributes
	// ============================================================================================================================

	FGameplayTag Attribute_Health;
	FGameplayTag Attribute_MaxHealth;
	FGameplayTag Attribute_FlowGauge;
	FGameplayTag Attribute_Adrenaline;

	// ============================================================================================================================
	// Effects
	// ============================================================================================================================

	FGameplayTag Effect_Combat;

	// ============================================================================================================================
	// Event Tags
	// ============================================================================================================================

	FGameplayTag Event_ModeSwitchReady;
	FGameplayTag Event_Ability_Attack;

	// ============================================================================================================================
	// Presage Result
	// ============================================================================================================================

	FGameplayTag Result_Hit;
	FGameplayTag Result_Dodge;

	// ============================================================================================================================
	// Data
	// ============================================================================================================================

	FGameplayTag Data_Amount;

	// ============================================================================================================================
	// Status
	// ============================================================================================================================

	FGameplayTag Status_Link;

private:
	static FWolfGameplayTags GameplayTags;
};