// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WolfGameMode.generated.h"

class ULevelCombatMode;
/**
 * 
 */
UCLASS()
class WOLFCORE_API AWolfGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Mode")
	TObjectPtr<ULevelCombatMode> LevelCombatMode;
};
