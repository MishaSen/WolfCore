// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/LevelScriptActor.h"
#include "WolfLevelScript.generated.h"

enum class ECombatMode : uint8;
/**
 * 
 */
UCLASS()
class WOLFCORE_API AWolfLevelScript : public ALevelScriptActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Mode")
	FGameplayTag StartingCombatTag;
};
