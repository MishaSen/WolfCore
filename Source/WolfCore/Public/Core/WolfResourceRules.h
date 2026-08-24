// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WolfResourceRules.generated.h"

/**
 * Per-channel gain rule for one resource channel (Flow Gauge or Adrenaline) on one hit channel
 * (damage dealt or damage taken). Designer-tunable via UWolfCombatSettings ("WolfCore|Resources").
 * Every value here is a placeholder — vision marks all numbers in the resource economy open.
 */
USTRUCT(BlueprintType)
struct WOLFCORE_API FResourceGainRule
{
	GENERATED_BODY()

	/** Per-hit base gain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Resources")
	float FlatAmount = 0.f;

	/** Additional gain equal to the coefficient applied to the hit's damage amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Resources")
	float PerDamageCoeff = 0.f;
};

/**
 * Result of a hit's resource gain computation. Consumed by the live application path, recorded by
 * the Presage bake for exact execution replay, and applied numerically during preview.
 */
USTRUCT(BlueprintType)
struct WOLFCORE_API FResourceGainResult
{
	GENERATED_BODY()

	/** Net change to apply to the player's FlowGauge for this hit (already mode-weighted). */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Resources")
	float FlowDelta = 0.f;

	/** Net change to apply to the player's Adrenaline for this hit (already mode-weighted). */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Resources")
	float AdrenalineDelta = 0.f;
};

/**
 * Central, pure resource-gain economy (ResourceLoop stage 1).
 *
 * ComputeResourceGain is deliberately PURE: no ASC reads, no world reads, no ambient mode access
 * — the caller passes the current mode explicitly. The truth contract of the resource loop in
 * this stage is that the live application path and the Presage bake's prediction path both call
 * this exact function, which is what keeps predicted and real resource gains identical. The only
 * external data consulted is the project's UWolfCombatSettings default object (designer constants,
 * identical in both paths) — config, not world or ASC state.
 */
class WOLFCORE_API FWolfResourceRules
{
public:
	/**
	 * Computes Flow Gauge and Adrenaline deltas for a single real hit.
	 *
	 * @param bPlayerDealtHit True if the hit SOURCE is player-controlled.
	 * @param bPlayerTookHit  True if the hit TARGET is player-controlled.
	 *   A self-hit (both true) legitimately contributes both the Dealt and Taken channels.
	 *   TODO(teams): when a team system exists, "player-SIDE dealt/taken" replaces "player
	 *   dealt/taken" at every classification site — this signature is unchanged; the callers pass
	 *   the classification in. Do not invent a team system here.
	 * @param DamageAmount The hit's primary magnitude. Sign-insensitive (used as-is for the
	 *   PerDamageCoeff term; callers pass an absolute value).
	 * @param CurrentMode  The mode to weight gains for (InputState.RT or InputState.TB). Passed as
	 *   a parameter — never read from the CMS here — precisely so the bake can pass InputState_TB
	 *   while simulating and RT code can pass the live mode.
	 * @return The combined per-channel deltas after the mode's major/minor weighting.
	 */
	static FResourceGainResult ComputeResourceGain(
		bool bPlayerDealtHit, bool bPlayerTookHit, float DamageAmount, const FGameplayTag& CurrentMode);

	/**
	 * Number of threshold stages the given soft cap unlocks — floor(MaxFlow / Interval).
	 *
	 * @param MaxFlow  The current soft cap (MaxFlowGauge). Values <= 0 yield 0 stages.
	 * @param Interval The stage width in Flow units (UWolfCombatSettings::FlowThresholdInterval).
	 * @return The stage count; 0 if either argument is non-positive.
	 *
	 * Pure (ResourceLoop stage 2) — reads only its parameters, exactly like ComputeResourceGain.
	 */
	static int32 GetStageCount(float MaxFlow, float Interval);

	/**
	 * The threshold stage a given Flow value is at, under a soft cap.
	 *
	 * Stage N is reached when FlowValue reaches N * Interval (floor(FlowValue / Interval)) — the
	 * "3s increments (3, 6, 9)" from vision's settled shape. The result is additionally clamped to
	 * GetStageCount(MaxFlow, Interval): banked Flow can't reach a stage the current soft cap hasn't
	 * unlocked. It can't exceed MaxFlow anyway post-clamping (PreAttributeChange), but the explicit
	 * clamp keeps the rule correct — and visibly so — if interval and cap ever drift apart.
	 *
	 * @param FlowValue The current (post-clamp) Flow value.
	 * @param MaxFlow    The current soft cap (MaxFlowGauge).
	 * @param Interval  The stage width (FlowThresholdInterval). Non-positive -> stage 0 always.
	 * @return The stage index, in [0, GetStageCount(MaxFlow, Interval)].
	 *
	 * Pure (ResourceLoop stage 2): reads only the caller's parameters.
	 */
	static int32 GetThresholdStage(float FlowValue, float MaxFlow, float Interval);
};