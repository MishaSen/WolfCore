# SimConfigLocationSpec.md

## Presage: Relocating `WolfSimConfig` and Making Step Size Designer-Tunable

**Status:** Proposed
**Owner:** WolfCore / Presage
**Touches:** `ActorSnapshot.h`, `WolfCombatSettings.h`, `CombatModeSubsystem.h/.cpp`
**Depends on:** `FrequencyFixSpec.md` (this spec assumes that fix is already in place — `CombatModeSubsystem::BakedStepSize` exists and is the single runtime source of truth for a bake's step size)

---

## 1. Background

`WolfSimConfig` currently lives in `ActorSnapshot.h` as a `namespace` of `static constexpr` values:

```cpp
namespace WolfSimConfig
{
    static constexpr float Frequency = 10.f;
    static constexpr float Step = 0.1f;
}
```

Two separate but related questions came up when reviewing the frequency-decoupling fix:

1. **Location:** `ActorSnapshot.h` is meant to describe the *shape* of a snapshot (`FActorSnapshot`). Simulation-rate configuration isn't data-shape — it's a policy value — so its presence there is a mild discoverability mismatch. Someone looking at `CombatModeSubsystem` (which is now the sole owner of bake-time step size, per the frequency fix) has no reason to think to look inside the snapshot-data header for where that value's default comes from.
2. **Tunability:** `constexpr` means changing the tick rate requires a recompile. The project already has a live pattern for exactly this kind of thing: `UWolfCombatSettings : public UDeveloperSettings`, which exposes `PresageEffectClass` and `ModeTimeDilationMap` as `Config, EditAnywhere` properties, editable in Project Settings and readable via `GetDefault<UWolfCombatSettings>()`.

These two questions turn out to have one answer: move the step size into `UWolfCombatSettings`, following the exact pattern already established by `PresageEffectClass`. This simultaneously fixes the location mismatch (settings live in the settings class, not the snapshot-data header) and gets designer-tunability for free (it's how every other value on that class already works — no new infrastructure needed).

`Frequency` (`= 1 / Step`) is dropped entirely rather than migrated, since nothing in the reviewed code reads it anymore — the frequency-fix work already replaced the one call site that used it (`RelativeTime * Frequency`) with `RelativeTime / StepSize`. Keeping both `Step` and `Frequency` as separately-editable settings would risk a designer changing one and forgetting the other; `Step` alone is sufficient and unambiguous.

---

## 2. Design

### 2.1 `WolfCombatSettings.h`

Add a new property, following the existing `Category = "WolfCore|Presage"` section:

```cpp
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
```

`ClampMin`/`UIMin` guards against a designer entering zero or a negative value in Project Settings, which would otherwise divide-by-zero or produce a negative/inverted step downstream.

### 2.2 `ActorSnapshot.h`

Remove the `WolfSimConfig` namespace entirely:

```cpp
// ============================================================================================================================
// Simulation Configuration Constants
// ============================================================================================================================

/** Namespace containing simulation configuration constants for temporal prediction and presage tick rates. */
namespace WolfSimConfig
{
    /** Simulation frequency in Hz defining the number of ticks per second during presage prediction. */
    static constexpr float Frequency = 10.f;

    /** Fixed time step in seconds between each simulation tick during temporal prediction. */
    static constexpr float Step = 0.1f;
}
```

— deleted. `ActorSnapshot.h` goes back to describing only the snapshot data shape.

### 2.3 `CombatModeSubsystem.h`

`BakedStepSize`'s default no longer references `WolfSimConfig::Step` (which no longer exists); it falls back to a plain literal matching `UWolfCombatSettings::PresageSimulationStep`'s default, used only if `GetDefault<UWolfCombatSettings>()` somehow returns null:

```cpp
private:
    /** The fixed step size (seconds) used to produce the current PredictionBuffer contents across
      * every combatant in the active bake. Set once at the start of ExecuteFutureBake() from
      * UWolfCombatSettings::PresageSimulationStep; every UWolfPresageComponent::GetSnapshotAtTime()
      * call reads this via GetBakedStepSize(), so a designer-tuned change to the simulation rate
      * only needs to update this one path. */
    float BakedStepSize = 0.1f; // Fallback only; overwritten from UWolfCombatSettings in SetMode().
```

### 2.4 `CombatModeSubsystem.cpp`

Update `SetMode` to read the designer-configured value instead of the removed compile-time constant:

```cpp
if (NewMode == WolfTag.InputState_TB)
{
    ...
    // Set the step size used for all PredictionBuffer index math during this bake.
    if (const auto* Settings = GetDefault<UWolfCombatSettings>())
    {
        BakedStepSize = FMath::Max(Settings->PresageSimulationStep, 0.01f);
    }
    FWolfPresageSimulator::ExecuteFutureBake(TrackedCombatants, MaxTimelineDuration, BakedStepSize);
    ScrubTimeline(0.f);
}
```

The `FMath::Max` guard is defense-in-depth alongside the `ClampMin` metadata — `ClampMin` only constrains the Project Settings UI, not values loaded from an `.ini` file that predates the clamp or was hand-edited.

`WolfCombatSettings.h` is already `#include`d in `CombatModeSubsystem.cpp` (used for `PresageEffectClass`), so no new include is required.

### 2.5 Other call sites

`WolfPresageComponent::ClearPredictionBuffer` / `GetSnapshotAtTime` are unaffected by this change — they already read through `GetCMS()->GetBakedStepSize()` per the frequency fix, with no direct reference to `WolfSimConfig`. Their existing `: WolfSimConfig::Step` fallback (used only if `GetCMS()` is null) must be updated to a literal, since the namespace is being removed:

```cpp
const float StepSize = GetCMS() ? GetCMS()->GetBakedStepSize() : 0.1f; // Fallback only; GetCMS() should not normally be null.
```

---

## 3. Non-Goals

- This does not introduce per-combatant or per-bake-phase variable step sizes. One value, one bake, shared by all combatants — same invariant as `FrequencyFixSpec.md`, just now designer-editable rather than compiled-in.
- This does not touch `WolfSimConfig::Frequency`'s conceptual meaning anywhere else in the codebase; a search confirms `GetSnapshotAtTime` was the only remaining consumer, and it was already migrated off `Frequency` onto `StepSize` division in the frequency fix.

---

## 4. Testing / Validation

1. **Default-value regression:** With `PresageSimulationStep` left at its default (`0.1f`), confirm bake/scrub behavior is bit-identical to before this change.
2. **Designer tuning check:** Change `PresageSimulationStep` in Project Settings (e.g. to `0.2f`), re-bake, and confirm `TotalSteps` in `ExecuteFutureBake` and the scrub index math in `GetSnapshotAtTime` both reflect the new value consistently (no leftover reference to the old compiled-in `0.1f` anywhere).
3. **Bad-value guard:** Set `PresageSimulationStep` to `0` or a negative value directly in the `.ini` (bypassing the `ClampMin` UI restriction) and confirm `FMath::Max` in `SetMode` prevents a divide-by-zero or negative step count.
4. **Compile check:** Confirm no remaining references to `WolfSimConfig::` anywhere in the codebase after `ActorSnapshot.h`'s namespace is deleted (a full-project search, not just the files touched here).

---

## 5. Rollout

Single PR. Existing `.ini` files with no `PresageSimulationStep` entry will simply use the `UPROPERTY`'s default (`0.1f`), matching current behavior — no migration step needed. No blueprint-facing API changes beyond the new editable settings field, which is additive.
