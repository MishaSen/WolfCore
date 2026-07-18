// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Presage/PresageOrchestratorTypes.h"
#include "Interfaces/IWolfCombatant.h"

class UCombatModeSubsystem;
class UBaseCombatAbility;
class UAbilitySystemComponent;

/**
 * Presage planning phase. Produces a finalized per-combatant Action Plan (as a flat list of
 * FIntentEntry, grouped by combatant when distributed) before FWolfPresageSimulator::ExecuteFutureBake
 * runs. Stage 6: two-round negotiation (disposition-weighted commit/defer + reactive reads).
 */
class WOLFCORE_API FPresageOrchestrator
{
public:
	/** Runs planning for the given combatants over the given duration, returning a flat intent
	  * ledger (not yet grouped per-combatant — see DistributePlan). Two-round negotiation:
	  * round 1 commits immediately, round 2 defers combatants react to what round 1 declared. */
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
	 * Selects an ability for the given AI-controlled combatant from its actual granted kit,
	 * filtered to UBaseCombatAbility subclasses that report CanActivateAbility() true right now
	 * (respecting real ActivationBlockedTags/ActivationRequiredTags, cooldown, and cost — the same
	 * gating real-time activation already uses). Among valid candidates, prefers ones whose
	 * ArchetypeTag doesn't already appear in Ledger against the same Target — i.e. avoids piling
	 * a redundant archetype onto a target that's already got one coming from another combatant
	 * (or from this same combatant's own earlier entry in this bake). Falls back to all valid
	 * candidates if every one of them would be redundant. Picks uniformly at random within
	 * whichever tier is used.
	 * Returns nullptr if the combatant has no ASC, or no currently-valid UBaseCombatAbility.
	 */
	static TSubclassOf<UBaseCombatAbility> DecideIntent(
		const TScriptInterface<IWolfCombatant>& Combatant,
		const AActor* Target,
		const TArray<FIntentEntry>& Ledger);

	/**
	 * Runs the chain-to-duration loop for one AI combatant: repeatedly calls DecideIntent and
	 * appends Declared entries to Ledger until TimeCursor reaches Duration or DecideIntent returns
	 * nullptr. Shared by both negotiation rounds — a round 1 committer and a round 2 committer run
	 * through exactly the same chaining logic, just at a different point in the pass.
	 */
	static void DeclareChainedIntents(
		UCombatModeSubsystem* CMS,
		const TScriptInterface<IWolfCombatant>& Combatant,
		AActor* Target,
		float Duration,
		TArray<FIntentEntry>& Ledger);

	/**
	 * True if any entry in Ledger already targets Target with the same ArchetypeTag as
	 * CandidateClass. An invalid (unset) ArchetypeTag on either side never counts as a match —
	 * only abilities that were both deliberately authored with the same archetype are considered
	 * redundant with each other.
	 */
	static bool IsArchetypeRedundantAgainstTarget(
		TSubclassOf<UBaseCombatAbility> CandidateClass,
		const AActor* Target,
		const TArray<FIntentEntry>& Ledger);
};