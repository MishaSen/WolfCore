# PresageOrchestratorStage4Spec.md

## Presage Orchestrator — Stage 4: Temporal Resolution & Interrupts

**Status:** Ready for implementation
**Scope:** Detect when one combatant's planned attack would land during another's windup, and resolve that interruption. Only the "take the hit" response needs to be mechanically functional — other response types (dodge/parry/feint) are data-driven placeholders, not implemented yet.
**Depends on:** stage 1 (`FInterruptResponseOption`, `FAbilityTimingProfile`), stage 3 (planning/execution plumbing this stage builds on)
**Touches:** `PresageOrchestratorTypes.h`, `PresageOrchestrator.h/.cpp`, `BaseCombatAbility.h`, `WolfPresageComponent.h/.cpp`, `CombatModeSubsystem.cpp`

This is stage 4 of 5. Round-based negotiation/deferral (disposition-weighted reactivity, section 3.1 of `PresageOrchestratorSpec.md`) is **not** part of this stage — it's bundled with stage 5's real AI decision content, since reactivity only makes sense once combatants have actual reasons to defer. This stage only adds: detecting an interrupt against whatever plan stage 3 already produced, and making the one required response (taking the hit) actually cut the interrupted ability short.

---

## 0. Correction to `PresageOrchestratorSpec.md` section 3.2

The original pseudocode's overlap condition was backwards:

```
for each other Entry2 in IntentLedger where Entry2.Target == Entry.Combatant:   // WRONG
```

This reads as "find entries that target the same actor Entry belongs to," which isn't the right question. What actually needs checking is: does `Entry`'s attack land on the combatant that `Entry` is targeting, during *that combatant's own* windup? The condition should key off `Entry2.Combatant`, not `Entry2.Target`:

```
for each other Entry2 in IntentLedger where Entry2.Combatant's actor == Entry.Target:   // corrected
```

This spec's section 2 below uses the corrected condition throughout.

---

## 1. Stub targeting extension (prerequisite for this stage to be testable)

Stage 3's `DecideIntent_Stub` chain never set `Target`, so without this, nothing in stage 3's plan can overlap with anything else. Extend `FPresageOrchestrator::RunPlanning`'s AI branch (in `PresageOrchestrator.cpp`) to assign a trivial target, mirroring the same "first other tracked combatant" fallback already used by `GatherPresageTargets` for players — this is still a stub, not real targeting logic, and should be commented as such:

```cpp
		// AI combatant: chain the stub ability back-to-back until it covers the full duration.
		// Real disposition/negotiation/interrupts are not implemented yet (stage 5 for negotiation;
		// interrupts as of this stage are detected but only "take the hit" is functional).
		AActor* StubTarget = nullptr;
		for (const auto& Other : Combatants)
		{
			const auto* OtherActor = Cast<AActor>(Other.GetObject());
			if (IsValid(OtherActor) && OtherActor != Actor)
			{
				StubTarget = const_cast<AActor*>(OtherActor);
				break;
			}
		}

		float TimeCursor = 0.f;
		while (TimeCursor < Duration)
		{
			const auto AbilityClass = DecideIntent_Stub();
			if (!AbilityClass) break;

			FIntentEntry Entry;
			Entry.Combatant = Combatant;
			Entry.AbilityClass = AbilityClass;
			Entry.Timing = CMS->GetOrComputeTimingProfile(AbilityClass);
			Entry.StartTime = TimeCursor;
			Entry.Status = EIntentStatus::Confirmed;
			Entry.Target = StubTarget; // STAGE 4 STUB — first other tracked combatant, not real targeting.
			Ledger.Add(Entry);

			TimeCursor += FMath::Max(Entry.Timing.TotalDuration, KINDA_SMALL_NUMBER);
		}
```

Only the `Entry.Target = StubTarget;` line and the `StubTarget` lookup above the loop are new; everything else in this block is unchanged from stage 3.

---

## 2. `PresageOrchestratorTypes.h` — extend `FIntentEntry`

Add two fields, directly below `UnavailableUntil`, following the same `bHas.../value` pattern already used there:

```cpp
	/** Only meaningful if bHasUnavailableUntil is true. Timeline time before which this combatant
	  * cannot be offered a new decision. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float UnavailableUntil = 0.f;

	/** True if this intent was interrupted during temporal resolution and InterruptedAtTime holds
	  * a meaningful value. Set by FPresageOrchestrator::ResolveInterrupts. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	bool bHasInterruptedAtTime = false;

	/** Only meaningful if bHasInterruptedAtTime is true. Timeline time at which the interrupting
	  * attack actually lands — Execution cuts this entry's ability short at this time instead of
	  * letting it run to its natural completion. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float InterruptedAtTime = 0.f;
```

---

## 3. `BaseCombatAbility.h` — per-ability interrupt response override

Deferred from stage 1 specifically to this stage (stage 1's spec named this as the reason not to add it early). Add directly below wherever `AbilitySequence` is declared:

```cpp
	/** Per-ability interrupt response options, used instead of UWolfCombatSettings::
	  * DefaultInterruptResponses when non-empty. See FInterruptResponseOption. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Presage")
	TArray<FInterruptResponseOption> InterruptResponses;
```

Add `#include "Presage/PresageOrchestratorTypes.h"` to `BaseCombatAbility.h`'s include block if not already present (it should already be there from stage 1's `ComputeAbilityTiming` addition).

---

## 4. `PresageOrchestrator.h` — new function

```cpp
	/** Groups the flat ledger by combatant, sorts each group by StartTime, and pushes each
	  * combatant's slice into its own UWolfPresageComponent via SetPlannedIntents(). Must be called
	  * before FWolfPresageSimulator::ExecuteFutureBake for the same bake. */
	static void DistributePlan(
		const TArray<TScriptInterface<IWolfCombatant>>& Combatants,
		const TArray<FIntentEntry>& Plan);

	/**
	 * Detects interrupts (one combatant's planned attack landing during another's windup) and
	 * resolves them. Only "take the hit" is mechanically functional this stage — any other
	 * authored FInterruptResponseOption is logged and falls back to the same behavior. Mutates
	 * Ledger in place; call after RunPlanning and before DistributePlan.
	 */
	static void ResolveInterrupts(TArray<FIntentEntry>& Ledger);

private:
	/** Returns the interrupt response table to use for the given entry: its ability's own
	  * InterruptResponses if non-empty, else UWolfCombatSettings::DefaultInterruptResponses.
	  * STAGE 4 SIMPLIFICATION: RequiredTags gating is not evaluated yet (no live tag-state model
	  * to check against) — only options with an empty RequiredTags container are considered
	  * available. This is safe today because only the always-available take-hit fallback is
	  * mechanically implemented regardless of which option gets picked; RequiredTags gating
	  * becomes meaningful once stage 5 adds real response types that need it.
	  */
	static TArray<FInterruptResponseOption> GetAvailableResponses(const FIntentEntry& InterruptedEntry);

	/** Weighted random pick among Options. Returns nullptr if Options is empty. */
	static const FInterruptResponseOption* PickWeightedResponse(const TArray<FInterruptResponseOption>& Options);
```

---

## 5. `PresageOrchestrator.cpp` — implementation

```cpp
void FPresageOrchestrator::ResolveInterrupts(TArray<FIntentEntry>& Ledger)
{
	// Map from victim index -> earliest attacker's AttackWindowStart that hits them. A victim can
	// overlap with more than one attacker; only the earliest one actually lands first and
	// interrupts them — later ones are moot for this entry (stacking further damage/hitstun
	// beyond the first hit is a separate, out-of-scope concern). No iteration cap is needed here:
	// that only becomes relevant once a *response* (e.g. a future dodge/parry) has its own active
	// window that could itself be interrupted by a second attacker — "take the hit" has no such
	// window, so there's nothing to recurse into yet.
	TMap<int32, float> EarliestAttackTime;

	for (int32 i = 0; i < Ledger.Num(); ++i)
	{
		const FIntentEntry& Attacker = Ledger[i];
		if (!Attacker.Target.IsValid()) continue;

		const float AttackWindowStart = Attacker.StartTime + Attacker.Timing.ActiveWindowStart;
		const float AttackWindowEnd = Attacker.StartTime + Attacker.Timing.ActiveWindowEnd;

		for (int32 j = 0; j < Ledger.Num(); ++j)
		{
			if (i == j) continue;

			const FIntentEntry& Victim = Ledger[j];
			const auto* VictimActor = Cast<AActor>(Victim.Combatant.GetObject());
			if (!IsValid(VictimActor) || VictimActor != Attacker.Target.Get()) continue;

			const float VictimWindupEnd = Victim.StartTime + Victim.Timing.WindupDuration;

			// Interval overlap: attacker's active window intersects victim's windup.
			if (AttackWindowStart < VictimWindupEnd && AttackWindowEnd > Victim.StartTime)
			{
				if (const float* Existing = EarliestAttackTime.Find(j))
				{
					if (AttackWindowStart < *Existing)
					{
						EarliestAttackTime[j] = AttackWindowStart;
					}
				}
				else
				{
					EarliestAttackTime.Add(j, AttackWindowStart);
				}
			}
		}
	}

	// Resolve each interrupted victim exactly once, using the earliest incoming attack.
	for (const auto& Pair : EarliestAttackTime)
	{
		const int32 VictimIndex = Pair.Key;
		const float EarliestAttackWindowStart = Pair.Value;

		FIntentEntry& Entry = Ledger[VictimIndex];

		const TArray<FInterruptResponseOption> Options = GetAvailableResponses(Entry);
		const FInterruptResponseOption* Chosen = PickWeightedResponse(Options);

		if (Chosen)
		{
			WOLF_LOG(Log, TEXT("[PRESAGE] Interrupt response chosen: %s (only take-hit is mechanically implemented this stage)"),
				*Chosen->ResponseTag.ToString());
		}

		// Only "take the hit" is functional — regardless of which option (if any) was chosen,
		// the mechanical effect this stage implements is: cut the interrupted ability short at
		// the moment the earliest interrupting attack actually lands.
		Entry.bHasInterruptedAtTime = true;
		Entry.InterruptedAtTime = FMath::Max(Entry.StartTime, EarliestAttackWindowStart);
		Entry.Status = EIntentStatus::Confirmed;
	}
}

TArray<FInterruptResponseOption> FPresageOrchestrator::GetAvailableResponses(const FIntentEntry& InterruptedEntry)
{
	TArray<FInterruptResponseOption> Result;

	const auto* AbilityCDO = InterruptedEntry.AbilityClass
		? InterruptedEntry.AbilityClass->GetDefaultObject<UBaseCombatAbility>()
		: nullptr;

	const TArray<FInterruptResponseOption>* Source = nullptr;
	if (AbilityCDO && AbilityCDO->InterruptResponses.Num() > 0)
	{
		Source = &AbilityCDO->InterruptResponses;
	}
	else if (const auto* Settings = GetDefault<UWolfCombatSettings>())
	{
		Source = &Settings->DefaultInterruptResponses;
	}

	if (Source)
	{
		for (const auto& Option : *Source)
		{
			if (Option.RequiredTags.IsEmpty()) // stage 4 simplification — see header comment
			{
				Result.Add(Option);
			}
		}
	}

	return Result;
}

const FInterruptResponseOption* FPresageOrchestrator::PickWeightedResponse(const TArray<FInterruptResponseOption>& Options)
{
	if (Options.Num() == 0) return nullptr;

	float TotalWeight = 0.f;
	for (const auto& Option : Options) TotalWeight += FMath::Max(Option.Weight, 0.f);
	if (TotalWeight <= 0.f) return &Options[0];

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (const auto& Option : Options)
	{
		Roll -= FMath::Max(Option.Weight, 0.f);
		if (Roll <= 0.f) return &Option;
	}
	return &Options.Last();
}
```

**Correction made after stage 4's first implementation pass, recorded here for history:** the version above supersedes an earlier draft that used a queue-and-cap approach (`InterruptedIndices` as a stack, popped with a `MaxInterruptIterations` bound). That approach had a real bug — if a victim overlapped with *multiple* attackers, it processed all of them in stack-pop (reverse-insertion) order rather than earliest-attack order, so `InterruptedAtTime` could end up set from whichever attacker happened to be iterated last, not whichever actually lands first. The corrected version tracks the single earliest `AttackWindowStart` per victim via `TMap<int32, float>` and resolves each victim exactly once. The cap is also correctly dropped: it was solving a problem that doesn't exist yet — a *response* (e.g. a future dodge) could itself have an active window that a second attacker interrupts, which is genuinely recursive and would need a bound, but "take the hit" has no window of its own, so nothing recurses. Reintroduce the cap specifically when a response type with its own duration is added.

---

## 6. `WolfPresageComponent.cpp` — `SimulateTick` cuts interrupted abilities short

Extend the existing exhaustion-check block (added in stage 3) to also check the active planned entry's interruption info:

```cpp
		// If this was our own planned/simulated ability (not a real one carried over from RT) and
		// its sequence just ran out, OR it's been interrupted and reached its cutoff time, free it
		// up so the next planned intent can take over.
		if (ActiveAbility == SimulatedAbility)
		{
			bool bExhausted = !ActiveAbility->GetAbilitySequence().IsValidIndex(ActiveAbility->GetCurrentPeriodIndex());

			bool bInterruptedNow = false;
			if (PlannedIntents.IsValidIndex(NextPlannedIntentIndex))
			{
				const FIntentEntry& CurrentEntry = PlannedIntents[NextPlannedIntentIndex];
				if (CurrentEntry.bHasInterruptedAtTime && SimElapsedTime >= CurrentEntry.InterruptedAtTime)
				{
					bInterruptedNow = true;
				}
			}

			if (bExhausted || bInterruptedNow)
			{
				bSimulatedAbilityActive = false;
				++NextPlannedIntentIndex;
			}
		}
```

This replaces the equivalent `if` block stage 3 added — same location in `SimulateTick`, same surrounding code, just the one condition widened from `bExhausted` alone to `bExhausted || bInterruptedNow`.

---

## 7. `CombatModeSubsystem.cpp` — wire `ResolveInterrupts` in

In both `SetMode`'s TB branch and `ReBakeTimeline`, directly between the existing `RunPlanning` and `DistributePlan` calls:

```cpp
	const auto Plan = FPresageOrchestrator::RunPlanning(this, TrackedCombatants, MaxTimelineDuration);
	// (Plan is non-const at this point so ResolveInterrupts can mutate it — adjust the `const auto`
	// from stage 3 to plain `auto` at both call sites.)
	FPresageOrchestrator::ResolveInterrupts(Plan);
	FPresageOrchestrator::DistributePlan(TrackedCombatants, Plan);
```

Note the `const auto Plan` from stage 3 needs to become non-const (`auto Plan`) at both call sites, since `ResolveInterrupts` takes a mutable reference.

---

## 8. Non-Goals for This Stage

- **Round-based negotiation/reactivity** (section 3.1 of the big spec) — bundled with stage 5.
- **Real response types.** Dodge/parry/feint are not implemented — any authored `FInterruptResponseOption` other than a conceptual "take the hit" still results in the same mechanical behavior (cut short), just with a log line noting the mismatch. Don't implement actual cancel-into-another-ability or dodge-movement logic this stage.
- **`RequiredTags` gating.** Only empty-`RequiredTags` options are considered "available" (see `GetAvailableResponses`'s comment) — there's no live tag-state model to check against yet. This becomes meaningful once stage 5 or later work adds real response types that need it.
- **Re-timing downstream entries after a cut-short interruption.** If an AI combatant's entry gets interrupted and cut short, its *next* planned entry's `StartTime` (computed by stage 3's stub chaining against the original, uninterrupted `TotalDuration`) is not recalculated — this can leave a small idle gap before the next entry's `StartTime` arrives. Accepted as a known limitation for this stage; proper handling means re-planning downstream entries in response to resolved outcomes, which is architecturally a stage 5+ concern (the "reactive to outcome" idea from the architecture doc).
- **Recursive interrupts** (a chosen response itself getting interrupted) — moot this stage since only one response (take-hit) exists and it doesn't introduce a new active window to check against.

---

## 9. Testing / Validation

1. **Basic interrupt detection.** Construct two AI combatants targeting each other with overlapping timing (engineer `DefaultPresageStubAbility`'s sequence and their relative `StartTime`s so one's active window overlaps the other's windup) — confirm the victim's entry ends up `Confirmed` with `bHasInterruptedAtTime = true` after `ResolveInterrupts`.
2. **No false positives.** Two combatants with non-overlapping timing, or with `Target` unset — confirm no entry gets marked `Interrupted`.
3. **Execution actually cuts short.** With an interrupted entry, confirm the baked `PredictionBuffer` shows that combatant's ability ending at `InterruptedAtTime`, not running to its natural full sequence length — this is the actual behavior this stage is supposed to deliver, not just a status flag.
4. **Multiple attackers on one victim resolve to the earliest hit.** Construct a 3-combatant scenario where two different attackers both overlap the same victim's windup at different times — confirm `InterruptedAtTime` ends up matching the *earlier* of the two attacks, not whichever happened to be iterated last. This is the specific bug the corrected implementation (section 5) fixes; worth testing directly rather than assuming the earlier draft's stack-pop order happened to work out.
5. **Per-ability override takes priority.** Author a non-empty `InterruptResponses` list on one ability and confirm `GetAvailableResponses` uses it instead of `UWolfCombatSettings::DefaultInterruptResponses` for entries using that ability.
6. **Empty tables are safe.** With no `FInterruptResponseOption`s authored anywhere (the common case today, since none exist yet), confirm `PickWeightedResponse` returns `nullptr` and the interrupted entry still resolves correctly (cut short) rather than crashing or being skipped.
