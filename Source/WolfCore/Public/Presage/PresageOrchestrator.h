// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Presage/PresageOrchestratorTypes.h"
#include "Interfaces/IWolfCombatant.h"

class UCombatModeSubsystem;
class UBaseCombatAbility;

/**
 * Presage planning phase. Produces a finalized per-combatant Action Plan (as a flat list of
 * FIntentEntry, grouped by combatant when distributed) before FWolfPresageSimulator::ExecuteFutureBake
 * runs. Stage 3: negotiation, interrupts, and real AI decision-making are not implemented yet — see
 * PresageOrchestratorStage3Spec.md for what this stage deliberately stubs.
 */
class WOLFCORE_API FPresageOrchestrator
{
public:
	/** Runs planning for the given combatants over the given duration, returning a flat intent
	  * ledger (not yet grouped per-combatant — see DistributePlan). */
	static TArray<FIntentEntry> RunPlanning(
		UCombatModeSubsystem* CMS,
		const TArray<TScriptInterface<IWolfCombatant>>& Combatants,
		float Duration);

	/** Groups the flat ledger by combatant, sorts each group by StartTime, and pushes each
	  * combatant's slice into its own UWolfPresageComponent via SetPlannedIntents(). Must be called
	  * before FWolfPresageSimulator::ExecuteFutureBake for the same bake. */
	static void DistributePlan(
		const TArray<TScriptInterface<IWolfCombatant>>& Combatants,
		const TArray<FIntentEntry>& Plan);

	/** Detects interrupts (one combatant's planned attack landing during another's windup) and
	  * resolves them. Only "take the hit" is mechanically functional this stage — any other
	  * authored FInterruptResponseOption is logged and falls back to the same behavior. Mutates
	  * Ledger in place; call after RunPlanning and before DistributePlan. */
	static void ResolveInterrupts(TArray<FIntentEntry>& Ledger);

private:
	/** Returns the interrupt response table to use for the given entry: its ability's own
	  * InterruptResponses if non-empty, else UWolfCombatSettings::DefaultInterruptResponses.
	  * STAGE 4 SIMPLIFICATION: RequiredTags gating is not evaluated yet (no live tag-state model
	  * to check against) — only options with an empty RequiredTags container are considered
	  * available. This is safe today because only the always-available take-hit fallback is
	  * mechanically implemented regardless of which option gets picked; RequiredTags gating
	  * becomes meaningful once stage 5 or later work adds real response types that need it. */
	static TArray<FInterruptResponseOption> GetAvailableResponses(const FIntentEntry& InterruptedEntry);

	/** Weighted random pick among Options. Returns nullptr if Options is empty. */
	static const FInterruptResponseOption* PickWeightedResponse(const TArray<FInterruptResponseOption>& Options);
	/**
	 * STAGE 3 STUB — returns UWolfCombatSettings::DefaultPresageStubAbility unconditionally. Real
	 * AI decision-making (constraint-gated ability selection, disposition-weighted negotiation) is
	 * stage 5. This function's only job right now is giving Execution something non-trivial to
	 * chain through, to validate the plumbing.
	 */
	static TSubclassOf<UBaseCombatAbility> DecideIntent_Stub();
};