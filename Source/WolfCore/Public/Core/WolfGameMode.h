// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WolfGameMode.generated.h"

/**
 * No plans to use. Flag for deletion.
 */
UCLASS()
class WOLFCORE_API AWolfGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Called at runtime when the game mode is ready to begin functioning. Overrides AGameModeBase::BeginPlay(). */
	virtual void BeginPlay() override;
};