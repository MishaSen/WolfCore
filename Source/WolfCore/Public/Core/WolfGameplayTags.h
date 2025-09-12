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

	// --- Ability Inputs ---
	FGameplayTag Input_Ability_1;
	FGameplayTag Input_Ability_2;
	FGameplayTag Input_Ability_3;
	FGameplayTag Input_Ability_4;

	// --- Mouse Inputs ---
	FGameplayTag Input_LMB;
	FGameplayTag Input_RMB;

	// Future extension: Add TMap<FGameplayTag, FGameplayTag> to link abilities to cooldowns, effects, etc.
	// e.g. TMap<FGameplayTag, FGameplayTag> AbilityCooldowns;

private:
	static FWolfGameplayTags GameplayTags;
};