// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/GameInstance.h"
#include "WolfGameInstance.generated.h"

/**
 * Custom Game Instance that tracks the selected combat mode as a Gameplay Tag.
 */
UCLASS()
class WOLFCORE_API UWolfGameInstance : public UGameInstance
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Combat Mode State
	// ============================================================================================================================

public:
	/** Currently active combat mode GameplayTag representing the player's selected combat state. */
	UPROPERTY(BlueprintReadWrite, Category = "WolfCore|Combat")
	FGameplayTag SelectedCombatMode;

	/** Clears the currently selected combat mode by resetting it to an empty GameplayTag. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Clear Selected Combat Mode"), Category = "WolfCore|Combat")
	void ClearSelectedCombatMode() { SelectedCombatMode = FGameplayTag(); }
};