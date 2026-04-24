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
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	FGameplayTag SelectedCombatMode;

	UFUNCTION(BlueprintCallable)
	void ClearSelectedCombatMode() { SelectedCombatMode = FGameplayTag(); }
};