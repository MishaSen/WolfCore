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

private:
	/**
	 * STAGE 3 STUB — returns UWolfCombatSettings::DefaultPresageStubAbility unconditionally. Real
	 * AI decision-making (constraint-gated ability selection, disposition-weighted negotiation) is
	 * stage 5. This function's only job right now is giving Execution something non-trivial to
	 * chain through, to validate the plumbing.
	 */
	static TSubclassOf<UBaseCombatAbility> DecideIntent_Stub();
};