// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "CombatModeListener.generated.h"

// ============================================================================================================================
// Interface Object
// ============================================================================================================================

/**
 * Interface object for combat mode listening. Implement to react when the active combat mode changes.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UCombatModeListener : public UInterface
{
	GENERATED_BODY()
};

// ============================================================================================================================
// Implementation Interface
// ============================================================================================================================

/**
 * Interface notified when the combat mode switches (e.g., RT <-> TB).
 * Implementing classes receive OnCombatModeChanged callbacks with the new mode tag.
 */
class WOLFCORE_API ICombatModeListener
{
	GENERATED_BODY()

public:
	// ============================================================================================================================
	// Mode Change Callbacks
	// ============================================================================================================================

	UFUNCTION(BlueprintNativeEvent, Category = "Combat")
	void OnCombatModeChanged(FGameplayTag NewMode);
};