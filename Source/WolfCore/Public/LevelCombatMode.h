// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CombatMode.h"
#include "Engine/DataAsset.h"
#include "LevelCombatMode.generated.h"

enum class ECombatMode : uint8;
/**
 * 
 */
UCLASS()
class WOLFCORE_API ULevelCombatMode : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Mode")
	ECombatMode DefaultMode = ECombatMode::OOC;
};
