// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "CharacterStatConfig.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API UCharacterStatConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	TMap<FGameplayTag, float> DefaultStats;
};