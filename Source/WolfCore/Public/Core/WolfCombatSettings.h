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
	/** Soft class pointer to the GameplayEffect applied when entering presage simulation mode. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Presage")
	TSoftClassPtr<UGameplayEffect> PresageEffectClass;

	// ============================================================================================================================
	// Combat Mode Configuration
	// ============================================================================================================================

	/** Map of combat mode GameplayTags to TimeDilation multipliers for temporal prediction scaling. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Combat", meta = (Categories = "InputState"))
	TMap<FGameplayTag, float> ModeTimeDilationMap;
};