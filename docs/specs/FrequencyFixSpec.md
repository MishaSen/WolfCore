# FrequencyFixSpec.md

## Presage: Prediction Buffer Frequency Decoupling

**Status:** Proposed
**Owner:** WolfCore / Presage
**Touches:** `WolfPresageComponent.h/.cpp`, `WolfPresageSimulator.h/.cpp`, `ActorSnapshot.h`

---

## 1. Problem

`UWolfPresageComponent::GetSnapshotAtTime` maps a relative time back to an index into `PredictionBuffer` using a global constant:

```cpp
const int32 Index = FMath::Clamp(
    FMath::RoundToInt(RelativeTime * WolfSimConfig::Frequency),
    0,
    PredictionBuffer.Num() - 1);
```

This is only correct if **every frame currently in `PredictionBuffer` was baked using exactly `WolfSimConfig::Step` (0.1s)**. That assumption is not enforced anywhere near the read site — it is enforced (if at all) only by convention at the call site of `FWolfPresageSimulator::ExecuteFutureBake`, which today happens to always pass `WolfSimConfig::Step` as its internal step size.

Nothing in the type system, the buffer itself, or the bake call prevents that from changing. If a future change introduces variable-rate baking (e.g. coarser steps at the tail of a long timeline for performance, or a difficulty setting that changes fidelity, or a per-actor override), `GetSnapshotAtTime` will silently compute the wrong index. There is no crash, no assert, no log — combatants restore to a plausible-looking but incorrect future frame. This is a correctness bug in the making, not a crash-now bug, which makes it worse: it will not be caught by casual testing.

**Severity:** Real, load-bearing. Cheap to fix now; expensive to debug later once several features assume the buffer is queryable by time.

---

## 2. Root Cause

Frequency is treated as **global, static knowledge** (`WolfSimConfig::Frequency`/`Step`, a `namespace` of `constexpr` values) rather than **provenance of the specific buffer being read**. The buffer (`TArray<FActorSnapshot>`) has no memory of how it was produced. Any code reading it has to trust an out-of-band assumption instead of asking the buffer itself.

---

## 3. Options Considered

### Option A — Store actual step size as bake-owned state (chosen)
Since the step size is invariant across all combatants for a given bake (by design — this is not intended to vary per-actor), it should be owned by whatever represents "a bake is happening," not duplicated per-actor. `CombatModeSubsystem` is that owner: it's the singleton that orchestrates `ExecuteFutureBake`, so it holds one `BakedStepSize` value, set once at bake-start. `UWolfPresageComponent::GetSnapshotAtTime` reads it via `GetCMS()->GetBakedStepSize()` — a call path the component already uses elsewhere, so this adds no new coupling. The buffer becomes correct relative to what actually happened at bake-time; `WolfSimConfig::Frequency` stops being load-bearing for reads.

- ✅ Small, localized change.
- ✅ Buffer becomes correct regardless of what the simulator does upstream, now or later.
- ✅ Ownership matches the actual invariant (one rate per bake, shared by all combatants) instead of implying per-actor flexibility that isn't wanted.
- ✅ No duplicated state across N combatants' components.
- ⚠️ Requires one new field + accessor on `CombatModeSubsystem`, and updating the bake entry point to set it once at bake-start.

### Option B — Remove the step-size parameter entirely; hard-lock to one global step
Make `ExecuteFutureBake` stop taking any step-size flexibility at all (it already effectively ignores any variability today), and add a `static_assert`/comment making `WolfSimConfig::Step` the one true source, referenced identically at both write and read sites.

- ✅ Zero new state, minimal diff.
- ❌ Doesn't fix the underlying issue, just removes the (currently unused) surface that could introduce it. Anyone who later needs variable-rate baking has to redo this work anyway.
- ❌ Still relies on "nobody changes the constant meaning between bake and read," just with fewer places to do so.

### Option C — Store explicit timestamps per snapshot instead of relying on index math
Give `FActorSnapshot` (or a wrapper) a `float Timestamp` field; `GetSnapshotAtTime` does a binary search / nearest-match over timestamps instead of arithmetic index derivation.

- ✅ Most robust long-term; supports genuinely variable-rate baking (not just "one rate per bake" but "rate can change mid-bake").
- ❌ Larger change: touches `FActorSnapshot` (a `USTRUCT` that's already serialized/replicated in spirit across the codebase), adds a search instead of O(1) index lookup, and is more invasive than the bug currently warrants.
- Recommendation: worth doing *if* variable-rate-within-a-single-bake ever becomes a real requirement. Overkill for the problem as it exists today.

**Decision: Option A, with step size owned by `CombatModeSubsystem` rather than per-actor.** It directly closes the correctness hole, is a contained diff, matches the actual invariant (one step size per bake, not per actor), and doesn't foreclose Option C later if requirements grow (a single bake-owned step size is a strict subset of the metadata Option C would need — moving to per-snapshot timestamps later is additive, not a reversal of this decision).

---

## 4. Design

### 4.1 `CombatModeSubsystem.h`

Add a private member and a public read accessor. This is the single shared value for the current (or most recent) bake — not duplicated per actor:

```cpp
/** The fixed step size (seconds) used to produce the current PredictionBuffer contents across
  * every combatant in the active bake. Set once at the start of ExecuteFutureBake(); every
  * UWolfPresageComponent::GetSnapshotAtTime() call reads this rather than WolfSimConfig::Step
  * directly, so a future change to the simulator's step size only needs to update this one path. */
float BakedStepSize = WolfSimConfig::Step;

public:
    float GetBakedStepSize() const { return BakedStepSize; }
```

### 4.2 `CombatModeSubsystem.cpp`

Set `BakedStepSize` once, at the point the bake actually begins (e.g. in `SetMode(TB)` or wherever `ExecuteFutureBake` is invoked from):

```cpp
void UCombatModeSubsystem::SetMode(ECombatMode NewMode)
{
    ...
    if (NewMode == ECombatMode::TurnBased)
    {
        BakedStepSize = WolfSimConfig::Step; // single write-time source of truth
        FWolfPresageSimulator::ExecuteFutureBake(TrackedCombatants, BakedStepSize, BakeDuration);
    }
    ...
}
```

### 4.3 `WolfPresageComponent.h` / `.cpp`

No new member needed. `ClearPredictionBuffer` keeps its existing signature (`MaxDuration` only) — it no longer needs a step-size parameter, since step size isn't actor-owned state:

```cpp
const FActorSnapshot* UWolfPresageComponent::GetSnapshotAtTime(float RelativeTime) const
{
    if (PredictionBuffer.Num() == 0) return nullptr;

    const float StepSize = GetCMS()->GetBakedStepSize();
    const int32 Index = FMath::Clamp(
        FMath::RoundToInt(RelativeTime / StepSize),
        0,
        PredictionBuffer.Num() - 1);
    return &PredictionBuffer[Index];
}
```

Note the switch from `RelativeTime * WolfSimConfig::Frequency` to `RelativeTime / GetCMS()->GetBakedStepSize()` — equivalent today (since `BakedStepSize` defaults to `WolfSimConfig::Step`), but now driven by what the subsystem actually used for this bake, not by a compile-time constant assumed to still apply.

This keeps `WolfSimConfig::Step` as the *single write-time source of truth* (referenced exactly once, in `CombatModeSubsystem`, at the point the bake actually happens), while every read path goes through the subsystem instead of the constant directly. If the step size ever needs to change (globally, for all combatants, at once — e.g. a difficulty or performance setting), this is the only line that needs to change; every `GetSnapshotAtTime` call is already correct by construction, with no per-actor state to keep in sync.

### 4.4 `ActorSnapshot.h`

No changes required. `WolfSimConfig::Frequency`/`Step` remain as the default/canonical bake rate; they simply stop being referenced from the per-actor read path (only `CombatModeSubsystem` references them now, at write-time).

---

## 5. Non-Goals

- This spec does **not** implement variable-rate baking. It only makes the buffer correct *if* someone changes the step size later, and removes the silent-corruption risk in the meantime.
- This spec does not address the sim/real divergence risk (separately discussed) or the `ApplyPresageDrainEffect` query-based lookup. Those are tracked separately.

---

## 6. Testing / Validation

1. **Regression check:** With no other changes, confirm `ScrubTimeline` behavior is bit-identical before/after (same indices produced for the same relative times), since `CombatModeSubsystem::BakedStepSize` defaults to `WolfSimConfig::Step`.
2. **Synthetic drift test:** Temporarily hardcode a different step value assigned to `BakedStepSize` in `CombatModeSubsystem::SetMode` (e.g. 0.05s) in a debug build, bake, and confirm every combatant's `GetSnapshotAtTime` returns the correct frame for known timestamps (e.g. `RelativeTime = 1.0s` returns index 20, not index 10) — and that all combatants agree, since the value is shared.
3. **Single-source check:** Confirm no other code path still reads `WolfSimConfig::Frequency`/`Step` directly for index math — `CombatModeSubsystem` should be the only write-time reference, `GetCMS()->GetBakedStepSize()` the only read-time reference.
4. **Guard:** Confirm `BakedStepSize` can't end up `<= 0` (e.g. an assert or clamp in `SetMode` before assigning it) rather than producing an infinite or negative-index buffer.

---

## 7. Rollout

Single self-contained PR. No data migration needed (buffer is `Transient`, rebuilt every bake). No blueprint-facing API changes (both touched functions are C++-only, non-`UFUNCTION`).
