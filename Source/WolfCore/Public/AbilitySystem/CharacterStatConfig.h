// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "CharacterStatConfig.generated.h"

/**
 * Data Asset that defines default stat values for a character, mapped by Gameplay Tag.
 */
UCLASS()
class WOLFCORE_API UCharacterStatConfig : public UDataAsset
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Default Statistics
	// ============================================================================================================================

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	TMap<FGameplayTag, float> DefaultStats;
};