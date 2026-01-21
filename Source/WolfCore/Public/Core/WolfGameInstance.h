// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/GameInstance.h"
#include "WolfGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API UWolfGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	FGameplayTag SelectedCombatMode;

	UFUNCTION(BlueprintCallable)
	void ClearSelectedCombatMode() { SelectedCombatMode = FGameplayTag(); }
};
