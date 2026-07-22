# PresageOrchestratorStage3Spec.md

## Presage Orchestrator — Stage 3: Planning/Execution Plumbing (No Interrupts, Stubbed AI)

**Status:** Ready for implementation
**Scope:** Wire a real planning phase into the bake pipeline and generalize Execution to consume a per-combatant plan instead of a single injected request. AI decision-making is a deliberate stub — the goal is proving the plumbing end-to-end, not real behavior.
**Depends on:** stage 1 (data model — first real callers of `ComputeAbilityTiming`/`GetOrComputeTimingProfile` appear here), stage 2 (both fixes are load-bearing for the chaining logic added in this stage), `PresageOrchestratorSpec.md` sections 3–4
**Touches:** new `PresageOrchestratorTypes.h`-adjacent files `PresageOrchestrator.h`/`.cpp`, `WolfCombatSettings.h`, `WolfPresageComponent.h/.cpp`, `CombatModeSubsystem.cpp`

This is stage 3 of 5. Rounds-based negotiation (deferring/reacting), interrupt detection and resolution, and real AI decision content are explicitly out of scope — stages 4 and 5. This stage's only job is: produce a plan, hand it to Execution, confirm Execution can walk through a multi-entry plan correctly (chaining), and confirm the existing player-injection flow still works unchanged from the player's perspective.

---

## 0. Two deliberate deviations from `PresageOrchestratorSpec.md`, and why

Worth flagging up front since the big-picture spec said something slightly different in both cases — this stage's design supersedes those two details, not the surrounding reasoning.

1. **`ExecuteFutureBake`'s signature does not change.** Section 4.1 of the original spec suggested adding a `Plan` parameter to `ExecuteFutureBake`. Instead, planning distributes each combatant's own slice of the plan directly into its `UWolfPresageComponent` (via a new `SetPlannedIntents` call) *before* `ExecuteFutureBake` runs at all. This keeps `FWolfPresageSimulator` completely untouched in this stage and avoids threading a global ledger through static functions that don't have easy per-combatant lookup context. Simpler, same effect.
2. **`TryInjectPresageAbility` does not need to change.** Section 4.2 suggested it "no longer needs to call `ReBakeTimeline()` directly." That was incorrect — it should keep calling `ReBakeTimeline()` exactly as it does today. What changes is what `ReBakeTimeline()` (and `SetMode`) do *internally* before executing: they now run planning first, and planning reads the player's stored request the same way it always has (`HasInjectedAbilityRequest()`). No change needed in `WolfAbilitySystemComponent.cpp` at all.

---

## 1. New files: `PresageOrchestrator.h` / `PresageOrchestrator.cpp`

Mirrors `FWolfPresageSimulator`'s shape — a stateless static-function class, not a `UObject`.

```cpp
// PresageOrchestrator.h
#pragma once

#include "CoreMinimal.h"
#include "Presage/PresageOrchestratorTypes.h"
#include "Interfaces/IWolfCombatant.h"

class UCombatModeSubsystem;

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
```

```cpp
// PresageOrchestrator.cpp
#include "Presage/PresageOrchestrator.h"
#include "Systems/CombatModeSubsystem.h"
#include "Combat/BaseCombatAbility.h"
#include "Core/WolfCombatSettings.h"
#include "GameFramework/Pawn.h"

TSubclassOf<UBaseCombatAbility> FPresageOrchestrator::DecideIntent_Stub()
{
	const auto* Settings = GetDefault<UWolfCombatSettings>();
	return Settings ? Settings->DefaultPresageStubAbility : nullptr;
}

TArray<FIntentEntry> FPresageOrchestrator::RunPlanning(
	UCombatModeSubsystem* CMS,
	const TArray<TScriptInterface<IWolfCombatant>>& Combatants,
	float Duration)
{
	TArray<FIntentEntry> Ledger;
	if (!CMS) return Ledger;

	for (const auto& Combatant : Combatants)
	{
		const auto* Actor = Cast<AActor>(Combatant.GetObject());
		if (!IsValid(Actor)) continue;

		auto* Presage = Combatant.GetInterface() ? Combatant.GetInterface()->GetPresageComponent() : nullptr;
		if (!IsValid(Presage)) continue;

		const auto* Pawn = Cast<APawn>(Actor);
		const bool bIsPlayer = Pawn && Pawn->IsPlayerControlled();

		if (bIsPlayer)
		{
			// Player's own intent comes from the existing injection mechanism, not the orchestrator.
			// At most one entry — Execution does not auto-continue the player past it (stage 3 scope).
			if (Presage->HasInjectedAbilityRequest())
			{
				const auto& Request = Presage->GetInjectedAbilityRequest();

				FIntentEntry Entry;
				Entry.Combatant = Combatant;
				Entry.AbilityClass = Request.AbilityClass;
				Entry.Timing = CMS->GetOrComputeTimingProfile(Request.AbilityClass);
				Entry.StartTime = Request.GetScheduledTime();
				Entry.Status = EIntentStatus::Confirmed;

				for (const auto& WeakTarget : Request.GetTargets())
				{
					if (WeakTarget.IsValid())
					{
						Entry.Target = WeakTarget;
						break;
					}
				}

				Ledger.Add(Entry);
			}
			continue;
		}

		// AI combatant: chain the stub ability back-to-back until it covers the full duration.
		// Real disposition/negotiation/interrupts are not implemented yet (stage 4/5).
		float TimeCursor = 0.f;
		while (TimeCursor < Duration)
		{
			const auto AbilityClass = DecideIntent_Stub();
			if (!AbilityClass) break; // nothing configured — leave this combatant with no plan.

			FIntentEntry Entry;
			Entry.Combatant = Combatant;
			Entry.AbilityClass = AbilityClass;
			Entry.Timing = CMS->GetOrComputeTimingProfile(AbilityClass);
			Entry.StartTime = TimeCursor;
			Entry.Status = EIntentStatus::Confirmed;
			Ledger.Add(Entry);

			// Guard against a zero-duration stub ability looping forever.
			TimeCursor += FMath::Max(Entry.Timing.TotalDuration, KINDA_SMALL_NUMBER);
		}
	}

	return Ledger;
}

void FPresageOrchestrator::DistributePlan(
	const TArray<TScriptInterface<IWolfCombatant>>& Combatants,
	const TArray<FIntentEntry>& Plan)
{
	for (const auto& Combatant : Combatants)
	{
		auto* Presage = Combatant.GetInterface() ? Combatant.GetInterface()->GetPresageComponent() : nullptr;
		if (!IsValid(Presage)) continue;

		TArray<FIntentEntry> ForThisCombatant = Plan.FilterByPredicate(
			[&Combatant](const FIntentEntry& Entry) { return Entry.Combatant == Combatant; });

		ForThisCombatant.Sort([](const FIntentEntry& A, const FIntentEntry& B) { return A.StartTime < B.StartTime; });

		Presage->SetPlannedIntents(ForThisCombatant);
	}
}
```

---

## 2. `WolfCombatSettings.h` — stub ability slot

Directly below `DefaultInterruptResponses` (added in stage 1):

```cpp
	/** STAGE 3 STUB ONLY. Placeholder ability every AI-controlled combatant repeatedly "chooses"
	  * for the full bake duration, used to validate the planning/execution plumbing end-to-end.
	  * Replaced by real AI decision-making in stage 5 — do not build gameplay content against this
	  * being the permanent mechanism. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Presage")
	TSubclassOf<UBaseCombatAbility> DefaultPresageStubAbility;
```

Add `class UBaseCombatAbility;` to the existing forward declarations if not already present (it already is — added in stage 1's `WolfCombatSettings.h` change).

---

## 3. `WolfPresageComponent.h` — plan storage and new accessors

In the "Public API - Ability Injection" section, directly below `HasInjectedAbilityRequest()`:

```cpp
	/** Returns whether a presage ability request is queued for simulation. */
	bool HasInjectedAbilityRequest() const { return InjectedAbilityRequest.AbilityClass != nullptr; }

	/** Returns the currently queued presage ability request. Only meaningful if
	  * HasInjectedAbilityRequest() is true. Read by FPresageOrchestrator::RunPlanning to fold the
	  * player's own intent into the same ledger AI combatants use. */
	const FPresageAbilityRequest& GetInjectedAbilityRequest() const { return InjectedAbilityRequest; }

	/** Sets this combatant's slice of the current bake's finalized Action Plan, replacing anything
	  * previously set, and resets playback to the first entry. Called once per bake by
	  * FPresageOrchestrator::DistributePlan, before ExecuteFutureBake runs. */
	void SetPlannedIntents(const TArray<FIntentEntry>& Intents);
```

Add `#include "Presage/PresageOrchestratorTypes.h"` to the existing include block.

In the private "Internal State" section, directly below `SimulatedAbility`:

```cpp
	/** Transient ability instance created for injected presage simulation. */
	UPROPERTY(Transient)
	TObjectPtr<UBaseCombatAbility> SimulatedAbility = nullptr;

	/** This combatant's slice of the current bake's Action Plan, set once per bake by
	  * FPresageOrchestrator::DistributePlan. Ordered by StartTime. */
	UPROPERTY(Transient)
	TArray<FIntentEntry> PlannedIntents;

	/** Index into PlannedIntents of the next entry to activate. Advanced by SimulateTick() once
	  * the current planned entry's simulated ability sequence is exhausted. */
	int32 NextPlannedIntentIndex = 0;
```

Rename the existing declaration:

```cpp
	/** Activates the injected ability once the scheduled time is reached. */
	void TryActivateInjectedAbility();
```

to:

```cpp
	/** Activates this combatant's next planned intent once its scheduled StartTime is reached. */
	void TryActivateNextPlannedIntent();
```

---

## 4. `WolfPresageComponent.cpp` — implementation

### 4.1 `SetPlannedIntents`

Add near `SetInjectedAbilityRequest`/`ClearInjectedAbilityRequest`:

```cpp
void UWolfPresageComponent::SetPlannedIntents(const TArray<FIntentEntry>& Intents)
{
	PlannedIntents = Intents;
	NextPlannedIntentIndex = 0;
}
```

Do not call this from `BeginSimulation()` or reset `PlannedIntents`/`NextPlannedIntentIndex` there — `SetPlannedIntents` is always called by `FPresageOrchestrator::DistributePlan` before `ExecuteFutureBake` (and therefore before `BeginSimulation()`) runs for the same bake, so `BeginSimulation()` touching these fields would risk wiping a plan that was just set, depending on call order. Leave `BeginSimulation()`/`EndSimulation()` exactly as stage 2 left them.

### 4.2 Replace `TryActivateInjectedAbility` with `TryActivateNextPlannedIntent`

```cpp
void UWolfPresageComponent::TryActivateNextPlannedIntent()
{
	if (bSimulatedAbilityActive) return;
	if (!PlannedIntents.IsValidIndex(NextPlannedIntentIndex)) return;

	const FIntentEntry& Entry = PlannedIntents[NextPlannedIntentIndex];
	if (SimElapsedTime < Entry.StartTime) return;
	if (!Entry.AbilityClass) return;

	SimulatedAbility = NewObject<UBaseCombatAbility>(
		this,
		Entry.AbilityClass,
		NAME_None,
		RF_Transient);

	const auto* AbilityCDO = Entry.AbilityClass->GetDefaultObject<UBaseCombatAbility>();
	TArray<FCombatPeriod> Sequence = AbilityCDO ? AbilityCDO->GetAbilitySequence() : TArray<FCombatPeriod>();

	UBaseCombatAbility::ResolveMoveToDestinations(
		Sequence,
		CharacterOwner->GetActorLocation(),
		Entry.Target.Get());

	SimulatedAbility->InitializeForSimulation(Sequence);
	bSimulatedAbilityActive = true;
	LastSimulatedPeriodIndex = 0;
	SyncSimulationMontage(SimulatedAbility);

	WOLF_LOG(Log, TEXT("[PRESAGE] Activated planned intent %d/%d: %s at t=%.2fs for %s"),
		NextPlannedIntentIndex + 1, PlannedIntents.Num(),
		*Entry.AbilityClass->GetName(), SimElapsedTime, *CharacterOwner->GetName());
}
```

Note this reads the ability's sequence fresh from `Entry.AbilityClass`'s CDO rather than from a stored copy on the entry itself — `FIntentEntry` (stage 1) deliberately does not carry a full `TArray<FCombatPeriod>`, only the class reference and derived timing, so there's exactly one place the sequence data lives.

### 4.3 `SimulateTick` — update the call site and add exhaustion-driven chaining

```cpp
void UWolfPresageComponent::SimulateTick(float Step)
{
	if (!CharacterOwner) return;

	SimElapsedTime += Step;
	TryActivateNextPlannedIntent();

	SimulatePhysicsStep(Step);
	SimulateAnimationStep(Step);

	// Advance period BEFORE snapshot capture so that both CreateSnapshot and
	// CurrentPeriodIndex assignment reflect the same post-advancement state.
	SimPeriodTime += Step;
	if (auto* ActiveAbility = GetActiveSimulationAbility())
	{
		const int32 PeriodBeforeAdvance = ActiveAbility->GetCurrentPeriodIndex();

		SimPeriodTime = FAbilityPeriodAdvancer::AdvancePeriod(
			ActiveAbility,
			SimPeriodTime,
			GetMoveComp()->MaxWalkSpeed,
			GetMoveComp()->MaxAcceleration,
			GetMoveComp()->Velocity.Size(),
			CharacterOwner->GetActorLocation()
		);

		// If this was our own planned/simulated ability (not a real one carried over from RT) and
		// its sequence just ran out, free it up so the next planned intent can take over. Mirrors
		// the stage 2 fix for the real-ability case, applied here to the simulated side.
		if (ActiveAbility == SimulatedAbility
			&& !ActiveAbility->GetAbilitySequence().IsValidIndex(ActiveAbility->GetCurrentPeriodIndex()))
		{
			bSimulatedAbilityActive = false;
			++NextPlannedIntentIndex;
		}
	}

	FActorSnapshot FutureFrame;
	if (auto* SnapshotControl = GetSnapshotControl())
	{
		ISnapshot::Execute_CreateSnapshot(SnapshotControl, FutureFrame);
	}

	FutureFrame.ActiveAbility = GetActiveSimulationAbility();
	FutureFrame.CurrentPeriodIndex = FutureFrame.ActiveAbility.IsValid()
		? FutureFrame.ActiveAbility->GetCurrentPeriodIndex()
		: -1;

	PredictionBuffer.Add(FutureFrame);
}
```

Only the marked block is new; everything else is unchanged from the current implementation (the commented-out debug log block can stay as-is or be removed — not part of this stage's scope either way).

Note the `ActiveAbility == SimulatedAbility` check deliberately excludes the real-ability case — stage 2's fix already handles a real ability's exhaustion by falling through to the simulated one in `GetActiveSimulationAbility()`; this stage only needs to additionally handle chaining *between* successive planned/simulated abilities, which stage 2 didn't need to (it only ever dealt with at most one).

---

## 5. `CombatModeSubsystem.cpp` — wire planning into both bake entry points

In `SetMode`'s TB branch, directly above the existing `ExecuteFutureBake` call:

```cpp
		// Set the step size used for all PredictionBuffer index math during this bake.
		if (const auto* Settings = GetDefault<UWolfCombatSettings>())
		{
			BakedStepSize = FMath::Max(Settings->PresageSimulationStep, 0.01f);
		}

		const auto Plan = FPresageOrchestrator::RunPlanning(this, TrackedCombatants, MaxTimelineDuration);
		FPresageOrchestrator::DistributePlan(TrackedCombatants, Plan);

		FWolfPresageSimulator::ExecuteFutureBake(TrackedCombatants, MaxTimelineDuration, BakedStepSize);
		ScrubTimeline(0.f);
```

In `ReBakeTimeline`, directly above its `ExecuteFutureBake` call:

```cpp
	const float ScrubTime = CurrentTimelineTime;

	const auto Plan = FPresageOrchestrator::RunPlanning(this, TrackedCombatants, MaxTimelineDuration);
	FPresageOrchestrator::DistributePlan(TrackedCombatants, Plan);

	FWolfPresageSimulator::ExecuteFutureBake(TrackedCombatants, MaxTimelineDuration, BakedStepSize);

	CurrentTimelineTime = -1.f;
	ScrubTimeline(ScrubTime);
```

Add `#include "Presage/PresageOrchestrator.h"` to `CombatModeSubsystem.cpp`'s existing include block.

---

## 6. Non-Goals for This Stage

- **No rounds, no negotiation, no disposition/personality weighting.** `RunPlanning` processes combatants independently, once each — section 3.1 of `PresageOrchestratorSpec.md` (deferral, round-based resolution) is not implemented here. That's stage 4/5 content.
- **No interrupt detection or resolution.** Sections 3.2/3.3 of the spec are not implemented — no timing-collision checks, no `FInterruptResponseOption` table is read anywhere yet (it exists from stage 1 as inert authoring surface only).
- **No real AI decision-making.** `DecideIntent_Stub` is explicitly a placeholder; do not make it "smarter" in this pass (e.g. don't have it pick based on distance to target, or vary by combatant) — that dilutes the point of stage 3, which is proving the plumbing works with the simplest possible decision function.
- **No real targeting for AI stub entries.** AI-chained `FIntentEntry`s leave `Target` unset (null). This is fine for validating plumbing; real targeting is separate, pre-existing scope (`GatherPresageTargets`, already flagged elsewhere as a placeholder).
- **`ScheduledTime` still comes from wherever the player's request set it** (unchanged from existing behavior — this stage doesn't touch `BuildInitialPresageRequest`).

---

## 7. Testing / Validation

1. **Player injection regression.** With no changes to the player-facing flow, confirm injecting an ability still works exactly as before stage 3 — single ability appears in the baked preview at the correct scheduled time.
2. **AI chaining works.** Configure `DefaultPresageStubAbility` to a real ability class with a non-trivial sequence (multiple periods). Confirm an AI-controlled combatant's baked timeline shows the stub ability repeating back-to-back for the full `MaxTimelineDuration` — i.e. `PredictionBuffer` shows the ability's montage/period cycling multiple times, not just once then going idle.
3. **No stub configured → inert, no crash.** Leave `DefaultPresageStubAbility` unset and confirm AI combatants simply have no plan and bake exactly as they did before this stage (idle/no ability), with no null-pointer issues in `RunPlanning` or `TryActivateNextPlannedIntent`.
4. **Zero-duration guard.** Configure a stub ability whose sequence sums to (or very near) zero duration and confirm `RunPlanning`'s while-loop terminates rather than hanging, thanks to the `KINDA_SMALL_NUMBER` floor.
5. **Rebake still works.** Trigger `ReBakeTimeline()` (via the existing player injection path) with an AI combatant present, and confirm the AI's stub chain is correctly rebuilt from `MasterStartSnapshot`, not appended to or duplicated from the previous bake — `SetPlannedIntents` unconditionally replacing `PlannedIntents` each call is what guarantees this; worth confirming directly.
6. **Stage 2 fixes remain correct alongside this stage's chaining.** With a real ability active at TB-entry (carried over from RT) that exhausts mid-bake, and an AI stub plan also present, confirm the real ability's exhaustion (stage 2's fix) and the simulated ability's exhaustion (this stage's new check) don't interfere with each other — the `ActiveAbility == SimulatedAbility` guard in section 4.3 should ensure only the correct one drives advancement of `NextPlannedIntentIndex`.
