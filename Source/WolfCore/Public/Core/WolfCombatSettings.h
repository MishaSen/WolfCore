#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Presage/PresageOrchestratorTypes.h"
#include "WolfCombatSettings.generated.h"

class UGameplayEffect;
class UBaseCombatAbility;

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

	/** Fixed time step, in seconds, between each simulation tick during future-bake prediction.
	  * Read once by CombatModeSubsystem at the start of each bake; all combatants in a given
	  * bake always share this single value (see FrequencyFixSpec.md). */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Presage", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float PresageSimulationStep = 0.1f;

	/** Project-wide default interrupt response options, used when an ability doesn't specify its
	  * own list. Not read by any logic yet — the Presage orchestrator's interrupt resolution
	  * (added in a later stage) is the intended consumer. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Presage")
	TArray<FInterruptResponseOption> DefaultInterruptResponses;

	// ============================================================================================================================
	// Combat Mode Configuration
	// ============================================================================================================================

	/** Map of combat mode GameplayTags to TimeDilation multipliers for temporal prediction scaling. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Combat", meta = (Categories = "InputState"))
	TMap<FGameplayTag, float> ModeTimeDilationMap;
};