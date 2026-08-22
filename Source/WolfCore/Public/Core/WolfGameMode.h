// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WolfGameMode.generated.h"

/**
 * No plans to use. Not deleted — referenced by Config/DefaultEngine.ini's GlobalDefaultGameMode
 * (points at BP_WolfGameMode, a Blueprint subclass of this class) and by the existing
 * WolfGameModeBase→WolfGameMode CoreRedirect in the same file. Revisit deletion if those
 * references are removed.
 */
UCLASS()
class WOLFCORE_API AWolfGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Called at runtime when the game mode is ready to begin functioning. Overrides AGameModeBase::BeginPlay(). */
	virtual void BeginPlay() override;
};