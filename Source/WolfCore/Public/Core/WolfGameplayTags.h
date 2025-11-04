// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * 
 */

struct FWolfGameplayTags
{
	static const FWolfGameplayTags& Get() { return GameplayTags; }
	static void AddNativeTag(FGameplayTag& Tag, const FName& TagName, const FString& Description);
	static void InitializeNativeGameplayTags();

	// --- Input Tags ---
	FGameplayTag Input_Ability1;
	FGameplayTag Input_Ability2;
	FGameplayTag Input_Ability3;
	FGameplayTag Input_Ability4;
	FGameplayTag Input_Jump;
	FGameplayTag Input_Primary;
	FGameplayTag Input_Secondary;
	FGameplayTag Input_Switch;

	// --- Input State Tags ---
	FGameplayTag InputState_RT;
	FGameplayTag InputState_TB;
	FGameplayTag InputState_OOC;

	// --- Event Tags ---
	FGameplayTag Event_ModeSwitchReady;

	// Future extension: Add TMap<FGameplayTag, FGameplayTag> to link abilities to cooldowns, effects, etc.
	// e.g. TMap<FGameplayTag, FGameplayTag> AbilityCooldowns;

private:
	static FWolfGameplayTags GameplayTags;
};