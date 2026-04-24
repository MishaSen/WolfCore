#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "WolfCombatSettings.generated.h"

class UGameplayEffect;

/**
 * Developer settings for combat-related configuration, exposed in Project Settings.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Wolf Combat Settings"))
class WOLFCORE_API UWolfCombatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Presage Configuration
	// ============================================================================================================================

public:
	UPROPERTY(Config, EditAnywhere, Category = "Presage")
	TSoftClassPtr<UGameplayEffect> PresageEffectClass;

	// ============================================================================================================================
	// Combat Mode Configuration
	// ============================================================================================================================

	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (Categories = "InputState"))
	TMap<FGameplayTag, float> ModeTimeDilationMap;
};