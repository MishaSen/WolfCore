// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/WolfResourceRules.h"

#include "Core/WolfCombatSettings.h"
#include "Core/WolfGameplayTags.h"

FResourceGainResult FWolfResourceRules::ComputeResourceGain(
	bool bPlayerDealtHit, bool bPlayerTookHit, float DamageAmount, const FGameplayTag& CurrentMode)
{
	const auto* Settings = GetDefault<UWolfCombatSettings>();
	FResourceGainResult Result;

	if (!Settings) return Result; // No config -> no gains; nothing to tune against.

	const float PositiveDamage = FMath::Max(DamageAmount, 0.f);
	const auto EvalRule = [PositiveDamage](const FResourceGainRule& Rule)
	{
		return Rule.FlatAmount + Rule.PerDamageCoeff * PositiveDamage;
	};

	// Sum the applicable channel rules. A hit where the player is the source contributes via the
	// Dealt rule; where the player is the target, via the Taken rule — a self-hit legitimately
	// contributes both (both flags are true for a player hitting themselves).
	float RawFlow = 0.f;
	float RawAdrenaline = 0.f;
	if (bPlayerDealtHit)
	{
		RawFlow += EvalRule(Settings->FlowGain_DamageDealt);
		RawAdrenaline += EvalRule(Settings->AdrenalineGain_DamageDealt);
	}
	if (bPlayerTookHit)
	{
		RawFlow += EvalRule(Settings->FlowGain_DamageTaken);
		RawAdrenaline += EvalRule(Settings->AdrenalineGain_DamageTaken);
	}

	// Mode weighting (vision's major/minor split). Anything that is not TB is treated as RT:
	// real-time hits accrue Flow as the major channel (OOC hits behave like RT hits in this
	// economy), and the TB mode is the only place Adrenaline is the major channel.
	const bool bTurnBased = CurrentMode == FWolfGameplayTags::Get().InputState_TB;
	const float FlowMultiplier      = bTurnBased ? Settings->TBFlowGainMultiplier      : Settings->RTFlowGainMultiplier;
	const float AdrenalineMultiplier = bTurnBased ? Settings->TBAdrenalineGainMultiplier : Settings->RTAdrenalineGainMultiplier;

	Result.FlowDelta = RawFlow * FlowMultiplier;
	Result.AdrenalineDelta = RawAdrenaline * AdrenalineMultiplier;
	return Result;
}