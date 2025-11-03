// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/WolfGameMode.h"

#include "LevelCombatMode.h"
#include "Debug/WolfDebug.h"
#include "Systems/CombatModeSubsystem.h"

void AWolfGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (auto* CombatModeSystem = GetWorld()->GetSubsystem<UCombatModeSubsystem>())
	{
		if (LevelCombatMode)
		{
			CombatModeSystem->SetCombatMode(LevelCombatMode->DefaultMode);
			WOLF_INFO(TEXT("Set CombatMode to default level mode: %s"), *UEnum::GetValueAsString(LevelCombatMode->DefaultMode));
		}
		else
		{
			WOLF_WARN(TEXT("No LevelCombatMode set for %s. Defaulting to combat mode OOC"), *GetWorld()->GetName());
			CombatModeSystem->SetCombatMode(ECombatMode::OOC);
		}
	}
	else
	{
		WOLF_ERROR(TEXT("Failed to get CombatModeSubsystem from world %s"), *GetWorld()->GetName());
	}
}
