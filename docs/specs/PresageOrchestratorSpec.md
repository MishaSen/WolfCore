# PresageOrchestratorSpec.md

## Presage Orchestrator — Implementation Spec (Phase 1)

**Status:** Ready to implement
**Extends:** `PresageOrchestratorArchitecture.md`
**Scope of this document:** a concrete, buildable slice of the orchestrator — planning without interrupts, the Intent Ledger, ability timing metadata, and the generalization of ability injection into a full plan-execution pipeline. Interrupt resolution and disposition-weighted rolls are deliberately out of scope here (see §9).

---

## 0. Why the line is drawn here

The architecture doc left four open questions (§8). Two of them — ability metadata source, and Intent Ledger shape — have answers that don't require any design discussion; they're resolved below by looking at what already exists in the codebase. The other two — the interrupt iteration bound, and how deep reactive chains should go — are genuinely gameplay-feel questions (how aggressive should AI interrupts feel, how far should a reactive chain cascade before it looks incoherent). Those are better tuned against a working, non-interrupting planner than decided on paper.

So Phase 1 builds everything that has an unambiguous answer: metadata, ledger, round-based negotiation capped at 2 rounds, tag-gated validity checks, and the execution handoff. It deliberately stops before interrupt detection. That's the next spec, written after Phase 1 is in the engine and you've watched a few bakes play out.

Phase 1 also folds in the two known ability-injection bugs, since fixing them *is* the execution-handoff generalization — there's no separate patch-then-refactor step.

---

## 1. Ability Timing Metadata

**Resolves architecture doc §8, Open Question 1.**

Nothing new needs to be authored. `FCombatPeriod` already carries `Type`, `Duration`, `HitDelay`, and `Range` per period, and `AbilitySequence` is already an ordered `TArray<FCombatPeriod>`. That's a complete timing description — windup/active/recovery are just periods tagged `Windup`/`Attack`/`Wait` in sequence. `UAbilityFrameData` (StartupTime/ActiveTime/RecoveryTime) is not wired into anything else in the codebase and should **not** be extended for this — it would be a second, unsynced source of truth for exactly the timing data `FCombatPeriod` already owns.

Add one derived-data helper to `UBaseCombatAbility`:

```cpp
// BaseCombatAbility.h — new struct, above UBaseCombatAbility

/**
 * Derived timing summary for an ability's sequence, computed from FCombatPeriod durations.
 * Used by the orchestrator's planning phase to reason about timing without simulating physics.
 */
USTRUCT(BlueprintType)
struct FAbilityTimingProfile
{
	GENERATED_BODY()

	/** Sum of Duration for all periods before the first Attack period. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float WindupDuration = 0.f;

	/** Timeline offset (from ability start) at which the first Attack period's hit resolves (Duration up to it + its HitDelay). */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float FirstImpactTime = 0.f;

	/** Sum of Duration for all periods from the first Attack period through the last. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float ActiveWindowDuration = 0.f;

	/** Sum of Duration for all periods after the last Attack period. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float RecoveryDuration = 0.f;

	/** WindupDuration + ActiveWindowDuration + RecoveryDuration. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float TotalDuration = 0.f;
};
```

```cpp
// UBaseCombatAbility — new public method

/** Computes a timing summary from AbilitySequence for planning-phase reasoning. Pure function of AbilitySequence; safe to call on the CDO. */
UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Get Timing Profile"), Category = "WolfCore|Presage")
FAbilityTimingProfile GetTimingProfile() const;
```

**Implementation notes for `GetTimingProfile()`:**
- Walk `AbilitySequence` once. Track `bool bSeenFirstAttack`.
- Before the first `EPeriodType::Attack`, accumulate into `WindupDuration`.
- `FirstImpactTime = WindupDuration + <that first Attack period's HitDelay>` (matches how `HandleAttackHitEvent` is actually scheduled via `Period.HitDelay` in `ExecuteAnimatedPeriod`/timer path — see `BaseCombatAbility.cpp`).
- From the first `Attack` period through the last `Attack` period (inclusive), accumulate into `ActiveWindowDuration`.
- Everything after the last `Attack` period accumulates into `RecoveryDuration`.
- Use `UBaseCombatAbility::GetPeriodDuration(Period)` (already exists) rather than reading `Period.Duration` directly, since that helper already resolves montage-vs-fallback duration correctly.
- `TotalDuration` is the sum of all three.

This is a pure read of existing data — no gameplay behavior changes, no risk to the current bake.

---

## 2. The Intent Ledger

**Resolves architecture doc §8, Open Question 4 — this is the confirmed shape for Phase 1.**

New files: `Source/WolfCore/Public/Presage/IntentLedger.h` / `Private/Presage/IntentLedger.cpp`.

```cpp
// IntentLedger.h

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "IntentLedger.generated.h"

/** Status of a declared intent as planning proceeds. */
UENUM()
enum class EIntentStatus : uint8
{
	Declared,   // Committed in round 1 or resolved in round 2; not yet interrupted (Phase 2 will add transitions out of this).
	Confirmed   // No further negotiation will touch this entry; final for this bake.
	// Interrupted is intentionally NOT added yet — Phase 2 introduces it alongside interrupt detection.
};

/**
 * A single declared intent on the negotiation ledger. Deliberately lightweight —
 * reactive combatants read AbilityArchetypeTag and timing, not full simulated state.
 */
USTRUCT()
struct FIntentEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Combatant = nullptr;

	/** Tag identifying the "nature" of the declared ability (e.g. Ability.Archetype.HeavyMelee) — set from the ability CDO, not derived at runtime. */
	UPROPERTY()
	FGameplayTag AbilityArchetypeTag;

	UPROPERTY()
	TSubclassOf<class UTBCombatAbility> AbilityClass;

	UPROPERTY()
	TWeakObjectPtr<AActor> Target = nullptr;

	/** Approximate start time on the bake timeline, in seconds from bake start. */
	UPROPERTY()
	float StartTime = 0.f;

	UPROPERTY()
	EIntentStatus Status = EIntentStatus::Declared;

	/** Derived from FAbilityTimingProfile::TotalDuration at declare time — when this combatant becomes free again. */
	UPROPERTY()
	float UnavailableUntil = 0.f;
};

/** Append-only ledger of declared intents for one planning pass. Not persisted between bakes. */
USTRUCT()
struct FIntentLedger
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FIntentEntry> Entries;

	/** Appends a new entry. Ledger is append-only during planning — entries are never removed, only added. */
	void Declare(const FIntentEntry& Entry) { Entries.Add(Entry); }

	/** All entries belonging to a given combatant, in declaration order. */
	TArray<const FIntentEntry*> GetEntriesFor(const AActor* Combatant) const;

	/** Whether Combatant has any Declared/Confirmed entry whose UnavailableUntil > AtTime. */
	bool IsCombatantBusyAt(const AActor* Combatant, float AtTime) const;
};
```

`AbilityArchetypeTag` is new: add one `UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Combat Data") FGameplayTag ArchetypeTag;` to `UBaseCombatAbility` alongside `StartupInputTag`/`HitEventTag`. This is what a reactive combatant reads to judge "what's coming" without touching the real ability object — it needs to be authored per-ability (windup/range/damage-type isn't inferable from a tag name, so document on the property that content authors should follow a `Ability.Archetype.*` naming convention, e.g. `Ability.Archetype.HeavyMelee`, `Ability.Archetype.Ranged`, `Ability.Archetype.Defensive`).

---

## 3. Planning Interface

**New interface**, kept separate from `IWolfCombatant` to preserve interface segregation — a player-controlled combatant supplies its plan via `FPresageAbilityRequest` injection and never needs to implement this; only AI-controlled combatants that participate in orchestrator planning do.

New files: `Source/WolfCore/Public/Interfaces/IWolfPresagePlanner.h` / no .cpp needed (pure interface, matches `IWolfCombatant` pattern — actually `IWolfCombatant` has no .cpp either, it's declared inline).

```cpp
// IWolfPresagePlanner.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IWolfPresagePlanner.generated.h"

struct FIntentLedger;
struct FIntentEntry;

UINTERFACE(BlueprintType)
class WOLFCORE_API UWolfPresagePlanner : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by AI-controlled combatants that participate in orchestrator planning.
 * Not implemented by player-controlled combatants — their intent comes from FPresageAbilityRequest instead.
 */
class WOLFCORE_API IWolfPresagePlanner
{
	GENERATED_BODY()

public:
	/// @brief Round 1: does this combatant want to commit now, or wait to see what others declare?
	/// @param Ledger Ledger state so far this planning pass (empty at the very start of round 1).
	/// @return True to defer to round 2.
	virtual bool WantsToDefer(const FIntentLedger& Ledger) const = 0;

	/// @brief Produces a committed intent given the current ledger state. Called in round 1 for
	/// combatants that don't defer, and in round 2 for combatants that did.
	/// @param Ledger Ledger state at the time of the call.
	/// @param BakeStartTime Timeline offset this combatant's intent should be declared relative to.
	/// @return A fully-populated FIntentEntry. AbilityClass must pass the tag-gate check (see §5) —
	/// implementations are expected to only propose abilities the combatant could currently activate.
	virtual FIntentEntry DeclareIntent(const FIntentLedger& Ledger, float BakeStartTime) const = 0;
};
```

**Phase 1 default implementation is intentionally simple:** `WantsToDefer` can return a static per-character bool (e.g. backed by a new `bool bIsReactiveDisposition` property on `AWolfEnemyBase`, or read from a blackboard key) rather than the probability-weighted roll described in the architecture doc §3.1. That roll is Phase 2 — wire the interface now so swapping the decision rule later doesn't touch the orchestrator or the ledger.

`DeclareIntent`'s job in Phase 1: pick one currently-activatable `UTBCombatAbility` (reuse whatever ability-selection logic already exists behind `BTTask_ActivateAbilityByTag` / the character's ability set), pick a target (existing blackboard `TargetActor`), and return an entry with `StartTime = BakeStartTime` (no offset — Phase 1 doesn't stagger round-2 committers relative to round-1 ones; that timing nuance is part of §3.2 of the architecture doc, which Phase 2 owns).

---

## 4. The Orchestrator

New files: `Source/WolfCore/Public/Core/PresageOrchestrator.h` / `Private/Core/PresageOrchestrator.cpp`. Static utility struct, matching `FWolfPresageSimulator`'s existing pattern.

```cpp
// PresageOrchestrator.h

#pragma once

#include "CoreMinimal.h"
#include "Presage/IntentLedger.h"

class IWolfCombatant;
struct FPlannedAction;

/** One entry in a combatant's finalized Action Plan. */
USTRUCT()
struct FPlannedAction
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<class UTBCombatAbility> AbilityClass;

	UPROPERTY()
	TWeakObjectPtr<AActor> Target = nullptr;

	UPROPERTY()
	float StartTime = 0.f;
};

/** Maps each planned combatant to its ordered list of actions for one bake. */
USTRUCT()
struct FActionPlan
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, FPlannedActionArray> Plans; // see note below on TMap value type
};

/**
 * Static utility for the Presage planning phase: round-based intent negotiation over an
 * abstract Intent Ledger, producing a finalized FActionPlan consumed by FWolfPresageSimulator.
 *
 * Phase 1: no interrupt detection. Planning is two rounds, hard-capped.
 */
struct WOLFCORE_API FPresageOrchestrator
{
	/**
	 * Runs the full planning phase for all combatants that implement IWolfPresagePlanner.
	 * Combatants that don't implement the interface (e.g. the player) are skipped — their
	 * plan is supplied separately via FPresageAbilityRequest and merged in by the caller.
	 *
	 * @param Combatants   All tracked combatants for this bake (CombatModeSubsystem::GetTrackedCombatants()).
	 * @param BakeStartTime Timeline offset planning is relative to (normally 0.f).
	 * @return Finalized FActionPlan, one entry per planner-implementing combatant that declared an intent.
	 */
	static FActionPlan RunPlanningPhase(const TArray<TScriptInterface<IWolfCombatant>>& Combatants, float BakeStartTime);

	/**
	 * Tag-gate check (architecture doc §3.3): true if AbilityClass's ActivationBlockedTags/
	 * ActivationRequiredTags/SourceRequiredTags are satisfied by Combatant's current ASC tags —
	 * the same check UGameplayAbility::CanActivateAbility uses internally, applied to the CDO
	 * since no real ability instance exists yet during planning.
	 */
	static bool IsAbilityValidForCombatant(const TScriptInterface<IWolfCombatant>& Combatant, TSubclassOf<UTBCombatAbility> AbilityClass);

private:
	/** Converts a finalized FIntentLedger into per-combatant FActionPlan entries. */
	static FActionPlan LedgerToActionPlan(const FIntentLedger& Ledger);
};
```

**Note on `FPlannedActionArray`:** `UPROPERTY() TMap<K, TArray<V>>` isn't supported by UHT. Either wrap `TArray<FPlannedAction>` in a small `USTRUCT FPlannedActionArray { UPROPERTY() TArray<FPlannedAction> Actions; }` (shown above, referenced but not spelled out — add it next to `FActionPlan`), or drop `UPROPERTY()` from `Plans` and use a plain (non-reflected) `TMap<TWeakObjectPtr<AActor>, TArray<FPlannedAction>>` since `FActionPlan` is transient, orchestrator-internal data that never needs to be edited in the editor or replicated. **Recommend the second option** — it's simpler and nothing here needs Blueprint exposure.

**`RunPlanningPhase` control flow:**

```cpp
FActionPlan FPresageOrchestrator::RunPlanningPhase(const TArray<TScriptInterface<IWolfCombatant>>& Combatants, float BakeStartTime)
{
	FIntentLedger Ledger;
	TArray<TScriptInterface<IWolfCombatant>> Deferred;

	// Round 1
	for (const auto& Combatant : Combatants)
	{
		auto* Planner = Cast<IWolfPresagePlanner>(Combatant.GetObject());
		if (!Planner) continue; // not a planning participant (e.g. player)

		if (Planner->WantsToDefer(Ledger))
		{
			Deferred.Add(Combatant);
			continue;
		}

		FIntentEntry Entry = Planner->DeclareIntent(Ledger, BakeStartTime);
		if (!IsAbilityValidForCombatant(Combatant, Entry.AbilityClass)) continue; // gate failed; combatant simply declares nothing this bake
		Ledger.Declare(Entry);
	}

	// Round 2 — hard-capped at one resolution pass in Phase 1 (architecture doc §8 Open Question 3;
	// deeper reactive chains are deferred until interrupt work in Phase 2 gives a reason to need them).
	for (const auto& Combatant : Deferred)
	{
		auto* Planner = Cast<IWolfPresagePlanner>(Combatant.GetObject());
		FIntentEntry Entry = Planner->DeclareIntent(Ledger, BakeStartTime);
		if (!IsAbilityValidForCombatant(Combatant, Entry.AbilityClass)) continue;
		Ledger.Declare(Entry);
	}

	return LedgerToActionPlan(Ledger);
}
```

This is a direct, literal implementation of architecture doc §3.1 rounds 1–2, without the timing-resolution/interrupt pass of §3.2.

---

## 5. Execution Handoff — Generalizing Ability Injection

This section replaces the single-slot injected-ability mechanism in `UWolfPresageComponent` with a queue driven by `FActionPlan`, and fixes both known bugs as part of the same change (there's no separate patch step — the bug-causing state *is* what's being replaced).

**Current state (`WolfPresageComponent.h`):** `InjectedAbilityRequest` (single `FPresageAbilityRequest`), `SimulatedAbility` (single ability pointer), `bSimulatedAbilityActive` (single bool). `HasInjectedAbilityRequest()` is checked by consumers; `ClearInjectedAbilityRequest()` exists but nothing calls it, so a stale request survives into the next bake. `GetActiveSimulationAbility()` returns `SimulatedAbility` unconditionally once `bSimulatedAbilityActive` is true — it never checks whether that ability's sequence has actually finished, so it permanently blocks any later entry.

**New state — replace the three fields above with:**

```cpp
// UWolfPresageComponent — replaces InjectedAbilityRequest / SimulatedAbility / bSimulatedAbilityActive

/** Queued plan for this bake — either the player's single-entry injected request, or the orchestrator's output for this combatant. */
UPROPERTY(Transient)
TArray<FPlannedAction> PlannedActions;

/** Index into PlannedActions of the entry currently executing, or INDEX_NONE if nothing has started yet / all entries are done. */
int32 CurrentPlanIndex = INDEX_NONE;

/** Transient ability instance for whichever plan entry is currently active. */
UPROPERTY(Transient)
TObjectPtr<UBaseCombatAbility> SimulatedAbility = nullptr;
```

**New/changed public API:**

```cpp
/** Replaces SetInjectedAbilityRequest — queues a full plan (or a single-entry plan, for the player's injected ability). */
void SetPlannedActions(const TArray<FPlannedAction>& InPlan);

/** Replaces ClearInjectedAbilityRequest — called from BeginSimulation() so no plan survives into a bake it wasn't produced for. */
void ClearPlannedActions();

bool HasPendingPlannedAction() const { return PlannedActions.IsValidIndex(CurrentPlanIndex + 1); }
```

**Bug fix 1 (stale request):** `ClearPlannedActions()` is called unconditionally at the top of `BeginSimulation()`, *before* the caller (see §6) calls `SetPlannedActions()` for the upcoming bake. This guarantees a plan never survives across bakes by accident — the previous code's problem was that clearing was a separate, easy-to-forget call; now it's structurally part of the same lifecycle method that already resets `SimElapsedTime`/`SimPeriodTime`/`LastSimulatedPeriodIndex`.

**Bug fix 2 (stale ability blocking progress):** rewrite `GetActiveSimulationAbility()`:

```cpp
UBaseCombatAbility* UWolfPresageComponent::GetActiveSimulationAbility() const
{
	if (!CharacterOwner) return nullptr;

	if (auto* RealAbility = CharacterOwner->GetActiveCombatAbility())
	{
		return RealAbility;
	}

	// A simulated ability is only "active" while its own sequence hasn't finished.
	// Once CurrentPeriodIndex runs past the end, it's stale — fall through so
	// TryActivateNextPlannedAction() can advance the queue instead of returning
	// an ability that will never progress again.
	if (SimulatedAbility && SimulatedAbility->GetAbilitySequence().IsValidIndex(SimulatedAbility->GetCurrentPeriodIndex()))
	{
		return SimulatedAbility;
	}

	return nullptr;
}
```

**Rename `TryActivateInjectedAbility()` → `TryActivateNextPlannedAction()`:**

```cpp
void UWolfPresageComponent::TryActivateNextPlannedAction()
{
	if (GetActiveSimulationAbility() != nullptr) return; // still mid-sequence on the current entry (real or simulated)
	if (!HasPendingPlannedAction()) return;

	const int32 NextIndex = CurrentPlanIndex + 1;
	const FPlannedAction& Next = PlannedActions[NextIndex];
	if (SimElapsedTime < Next.StartTime) return;

	SimulatedAbility = NewObject<UBaseCombatAbility>(this, Next.AbilityClass, NAME_None, RF_Transient);

	TArray<FCombatPeriod> Sequence = SimulatedAbility->GetAbilitySequence(); // CDO sequence, copied
	UBaseCombatAbility::ResolveMoveToDestinations(Sequence, CharacterOwner->GetActorLocation(), Next.Target.Get());

	SimulatedAbility->InitializeForSimulation(Sequence);
	CurrentPlanIndex = NextIndex;
	SyncSimulationMontage(SimulatedAbility);

	WOLF_LOG(Log, TEXT("[PRESAGE] Plan entry %d: %s at t=%.2fs for %s"),
		CurrentPlanIndex, *Next.AbilityClass->GetName(), SimElapsedTime, *CharacterOwner->GetName());
}
```

Call site: everywhere `TryActivateInjectedAbility()` was previously called from `SimulateTick`/`SimulatePhysicsStep`, call `TryActivateNextPlannedAction()` instead — same call site, same timing, just re-checked every step so a multi-entry plan advances automatically as each entry's sequence exhausts (which is exactly what bug fix 2 now allows).

**`BeginSimulation()` / `EndSimulation()` updates:**

```cpp
void UWolfPresageComponent::BeginSimulation()
{
	ClearPlannedActions(); // bug fix 1 — see above
	SimElapsedTime = 0.f;
	SimPeriodTime = 0.f;
	CurrentPlanIndex = INDEX_NONE;
	SimulatedAbility = nullptr;
	LastSimulatedPeriodIndex = -1;
}
```

Note the ordering: `ClearPlannedActions()` runs first thing in `BeginSimulation()`, and the *caller* (CombatModeSubsystem, §6) must call `SetPlannedActions()` on each combatant's presage component only *after* `BeginSimulation()` would otherwise run again — in practice this means: caller populates plans, then `FWolfPresageSimulator::ExecuteFutureBake` runs `SetupCombatantSimulation` → `BeginSimulation()`. So actually **flip the dependency**: keep the clear inside `BeginSimulation()`, but have the caller set the plan via `SetPlannedActions()` *before* calling `ExecuteFutureBake` at all, and have `BeginSimulation()` clear only if no plan was set this cycle... 

**Simplify:** don't clear inside `BeginSimulation()`. Instead, `SetPlannedActions()` is the only writer of `PlannedActions`, and it unconditionally overwrites (`PlannedActions = InPlan;`), so there's no "stale from last bake" case as long as the caller always calls `SetPlannedActions()` (with an empty array, if the combatant has nothing planned) for every tracked combatant before every bake. Make that contract explicit in the doc comment on `SetPlannedActions`, and have `CombatModeSubsystem` do it unconditionally in step order (§6) rather than only for combatants that have something to inject. This is a cleaner fix than a clear-on-begin/clear-on-end pair that has to be called at exactly the right moment — it removes the "never called" failure mode by removing the code path that needed the call at all.

---

## 6. Integration Points in `CombatModeSubsystem`

Both `SetMode(TB)` and `ReBakeTimeline()` currently call `FWolfPresageSimulator::ExecuteFutureBake(TrackedCombatants, MaxTimelineDuration, BakedStepSize)` directly. Insert planning immediately before that call, in both places:

```cpp
// New private helper on UCombatModeSubsystem
void UCombatModeSubsystem::RunPresagePlanningAndBake()
{
	// 1. Orchestrator plans for every AI combatant that implements IWolfPresagePlanner.
	const FActionPlan Plan = FPresageOrchestrator::RunPlanningPhase(TrackedCombatants, 0.f);

	// 2. Push each combatant's plan into its presage component. Every tracked combatant gets a call —
	//    empty array if nothing was planned for them this bake — so PlannedActions is always fresh (§5).
	for (auto& Combatant : TrackedCombatants)
	{
		auto* Presage = Combatant.GetInterface()->GetPresageComponent();
		if (!Presage) continue;

		if (const auto* Entry = Plan.Plans.Find(Cast<AActor>(Combatant.GetObject())))
		{
			Presage->SetPlannedActions(*Entry);
		}
		else
		{
			Presage->SetPlannedActions({});
		}
	}

	// 3. Player's own injected ability (from TryInjectPresageAbility) overrides whatever step 2 set
	//    for the player's presage component — the player doesn't implement IWolfPresagePlanner, so
	//    step 1 never produced an entry for them; this call is the player's only source of a plan.
	if (PendingPlayerAbilityRequest.IsSet())
	{
		FPlannedAction PlayerAction;
		PlayerAction.AbilityClass = PendingPlayerAbilityRequest->AbilityClass;
		PlayerAction.StartTime = PendingPlayerAbilityRequest->GetScheduledTime();
		// Target resolution: first valid entry in PendingPlayerAbilityRequest->GetTargets(), matching
		// the existing TryActivateInjectedAbility target-resolution loop.
		GetPlayerPresageComponent()->SetPlannedActions({ PlayerAction });
	}

	FWolfPresageSimulator::ExecuteFutureBake(TrackedCombatants, MaxTimelineDuration, BakedStepSize);
}
```

Replace both existing direct calls to `ExecuteFutureBake` (in `SetMode` and `ReBakeTimeline`) with `RunPresagePlanningAndBake()`.

**Note:** `PendingPlayerAbilityRequest` / `GetPlayerPresageComponent()` above are named to match the *existing* ability-injection call path — whatever currently calls `SetInjectedAbilityRequest()` on the player's presage component (search the codebase for the actual call site, likely in `WolfPlayerController` or wherever `TryInjectPresageAbility` lives) should be updated to store the request here and call `SetPlannedActions()` with a single-entry array instead, following the same pattern as step 2. The exact field/accessor names should match whatever's already there rather than being invented fresh.

---

## 7. Files Touched — Summary

| File | Change |
|---|---|
| `Abilities/BaseCombatAbility.h/.cpp` | Add `FAbilityTimingProfile`, `GetTimingProfile()`, `ArchetypeTag` property |
| `Presage/IntentLedger.h/.cpp` | **New.** `EIntentStatus`, `FIntentEntry`, `FIntentLedger` |
| `Interfaces/IWolfPresagePlanner.h` | **New.** Planning interface |
| `Core/PresageOrchestrator.h/.cpp` | **New.** `FPlannedAction`, `FActionPlan`, `FPresageOrchestrator` |
| `Core/WolfPresageComponent.h/.cpp` | Replace `InjectedAbilityRequest`/`SimulatedAbility`/`bSimulatedAbilityActive` with `PlannedActions`/`CurrentPlanIndex`; rewrite `GetActiveSimulationAbility()`; rename `TryActivateInjectedAbility()` → `TryActivateNextPlannedAction()`; replace `SetInjectedAbilityRequest`/`ClearInjectedAbilityRequest` with `SetPlannedActions`/remove the clear (see §5) |
| `Systems/CombatModeSubsystem.h/.cpp` | New `RunPresagePlanningAndBake()`; both `SetMode` and `ReBakeTimeline` call it instead of `ExecuteFutureBake` directly |
| `Character/WolfEnemyBase.h/.cpp` (or wherever AI combatants live) | Implement `IWolfPresagePlanner`; add whatever backs `bIsReactiveDisposition` for Phase 1's simplified `WantsToDefer` |

`FPresageAbilityRequest` itself is unchanged — it remains the player-injection data type; it's converted into a one-entry `FActionPlan` contribution at the integration point in §6 rather than being restructured.

---

## 8. Suggested Build Order

1. `FAbilityTimingProfile` / `GetTimingProfile()` — no dependents, easy to unit-verify against known ability assets.
2. `IntentLedger.h` — pure data, no dependents yet.
3. `IWolfPresagePlanner` + a Phase-1 `WantsToDefer`/`DeclareIntent` implementation on one enemy class, to have something concrete to plan against.
4. `FPresageOrchestrator` — build and test `RunPlanningPhase` in isolation (log the resulting `FActionPlan` rather than wiring it in yet).
5. `WolfPresageComponent` rewrite (§5) — this is the riskiest step since it touches the existing bake path; verify the two bug fixes independently (a plan with 2+ entries should visibly advance past the first ability in a scrub, which the old code couldn't do).
6. `CombatModeSubsystem` integration (§6) — wire it all together last, once 1–5 are individually verified.

---

## 9. Explicitly Deferred (Phase 2+)

- **Interrupt detection & resolution** (architecture doc §3.2) — `FInterruptResponseOption` weighted table, the iterative resolve-cascading-responses loop, and its termination bound.
- **Probability-weighted disposition rolls** replacing Phase 1's static `bIsReactiveDisposition`.
- **Timing-aware round resolution** — Phase 1 doesn't compute "who lands a hit on whom in what order" from committed intents; it just declares intents. That computation is what interrupt detection needs, so it arrives together with Phase 2.
- **DoT/HoT-aware predicted attributes** (architecture doc §5) — untouched by this document; still fully deferred.
- Round cap beyond 2, if content ends up needing deeper reactive chains than Phase 1's cap allows.
