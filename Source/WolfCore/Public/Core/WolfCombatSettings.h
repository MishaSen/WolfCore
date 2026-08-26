#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Core/WolfResourceRules.h"
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

	// ============================================================================================================================
	// Resource Gain Configuration (ResourceLoop stage 1)
	// ============================================================================================================================

	/** Flow Gauge gained per real hit DEALT by the player (flat + per-damage coefficient).
	  * All placeholder values — vision marks every number in the resource economy open. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources")
	FResourceGainRule FlowGain_DamageDealt;

	/** Flow Gauge gained per real hit TAKEN by the player (flat + per-damage coefficient). */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources")
	FResourceGainRule FlowGain_DamageTaken;

	/** Adrenaline gained per real hit DEALT by the player (flat + per-damage coefficient). */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources")
	FResourceGainRule AdrenalineGain_DamageDealt;

	/** Adrenaline gained per real hit TAKEN by the player (flat + per-damage coefficient). */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources")
	FResourceGainRule AdrenalineGain_DamageTaken;

	/** Real time: Flow Gauge is the MAJOR gain channel (vision's bidirectional hook — RT builds
	  *  the resource TB spends). */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources")
	float RTFlowGainMultiplier = 1.0f;

	/** Real time: Adrenaline gain is the MINOR channel. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources")
	float RTAdrenalineGainMultiplier = 0.25f;

	/** Turn based: Flow Gauge gain is the MINOR channel. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources")
	float TBFlowGainMultiplier = 0.25f;

	/** Turn based: Adrenaline gain is the MAJOR channel (vision's bidirectional hook — TB builds
	 *  the resource RT spends). */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources")
	float TBAdrenalineGainMultiplier = 1.0f;

	// ============================================================================================================================
	// Flow Gauge Thresholds (ResourceLoop stage 2)
	// ============================================================================================================================

	/** Distance between threshold stages, in Flow units — stage N is reached at N x this value
	  * (the "3s increments" from vision's settled shape). Consumed by
	  * FWolfResourceRules::GetThresholdStage/GetStageCount and the CMS's stage-crossing
	  * telemetry. Placeholder — vision marks these numbers open. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources", meta = (ClampMin = "0.0"))
	float FlowThresholdInterval = 3.f;

	/** ResourceLoop Stage 3 — the stage-quantized TB budget is Stage x FlowThresholdInterval
	  * seconds (unit identity: 1 Flow unit == 1s of prospective TB budget, per stage 2). If
	  * tuning ever wants to decouple Flow from seconds, the single conversion-multiplier seam is
	  * UCombatModeSubsystem::ResolveTBEntryBudget (see there) — do not add a multiplier
	  * speculatively. */

	/** Dev-convenience escape hatch for the stage-0 TB-entry gate (UCombatModeSubsystem::SetMode):
	  * when true, a player with banked Flow below the first threshold may STILL enter TB and is
	  * granted a stage-1-equivalent duration instead of being refused. Ship-intent default is
	  * false (banking-before-entering is a real decision). This gate's refusal of the player's
	  * switch input needs eventual feedback (UI/SFX), out of scope in ResourceLoopStage3 — flagged
	  * as a designer decision Shane may want to revisit once switch-feel is playtestable. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources")
	bool bAllowZeroStageTBEntry = false;

	/** Hard ceiling on the soft cap itself. Clamped in
	  * UWolfAttributeSet::PreAttributeChange (and mirrored in PostGameplayEffectExecute) so no
	  * StatConfig default or progression GE can raise MaxFlowGauge above it. Placeholder —
	  * vision's floated ceiling. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Resources", meta = (ClampMin = "0.0"))
	float FlowHardCeiling = 15.f;

	// ============================================================================================================================
	// TB Execution Configuration (PresagePreview stage 1)
	// ============================================================================================================================

	/** Applied to each hard-exit victim when TB is exited due to ETBExitReason::DamageTaken (see
	  * UCombatModeSubsystem::ExitTB). Unset by default — the debuff asset is authored later;
	  * ExitTB applies it if set and logs-and-skips if unset. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Presage")
	TSoftClassPtr<UGameplayEffect> TBHardExitDebuffClass;

	/** ArchetypeTag value that marks an ability as an "ending action" for TB execution — checked
	  * by UCombatModeSubsystem::CheckEndingAction against the player's currently-executing baked
	  * ability. Leave unset (invalid) to disable ending-action recognition entirely. Author using
	  * the same Ability.Archetype.* convention as ArchetypeTag elsewhere (e.g.
	  * Ability.Archetype.Ending) — no native tag registration needed. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Presage")
	FGameplayTag TBEndingActionArchetypeTag;
};