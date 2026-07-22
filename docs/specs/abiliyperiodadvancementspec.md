# Spec: Ability Period Advancement Extraction

## Problem

Ability period advancement logic is embedded inside `UWolfPresageComponent::SimulateTick()`. This creates two violations:

1. **SRP breach** — `UWolfPresageComponent` is responsible for physics simulation, animation scrubbing, prediction buffer management, *and* ability lifecycle progression. Period advancement is an ability concern, not a presage concern.
2. **Encapsulation breach** — `WolfPresageComponent.cpp` directly reads and mutates `UBaseCombatAbility` internals: `AbilitySequence`, `GetCurrentPeriodIndex()`, `SetCurrentPeriodIndex()`, and `CalculateMovementDuration()`. The presage component should not own this knowledge.

### Offending Block

```cpp
// WolfPresageComponent.cpp — SimulateTick()
if (bSequenceIsActive)
{
    if (IsValid(ActiveAbility))
    {
        const auto& AbilitySequence = ActiveAbility->AbilitySequence;
        const auto CurrentIndex = ActiveAbility->GetCurrentPeriodIndex();
        if (AbilitySequence.IsValidIndex(CurrentIndex))
        {
            auto Duration = 0.f;
            auto& CurrentPeriod = AbilitySequence[CurrentIndex];
            if (CurrentPeriod.Type == EPeriodType::MoveTo)
            {
                Duration = ActiveAbility->CalculateMovementDuration(...);
            }
            else Duration = ActiveAbility->GetPeriodDuration(CurrentPeriod);

            if (SimPeriodTime >= Duration)
            {
                const auto NextIndex = CurrentIndex + 1;
                ActiveAbility->SetCurrentPeriodIndex(NextIndex);
                SimPeriodTime = 0.f;
                // logging...
            }
        }
    }
}
else
{
    FutureFrame.ActiveAbility = nullptr;
    FutureFrame.CurrentPeriodIndex = -1;
}
```

---

## Goal

Extract the period advancement logic into a dedicated static utility struct `FAbilityPeriodAdvancer`, with `UBaseCombatAbility` owning a method that advances its own period given an elapsed time. `SimulateTick()` becomes a single delegating call with no knowledge of ability internals.

---

## New Type: `FAbilityPeriodAdvancer`

**Location:** `WolfCore/Public/Abilities/AbilityPeriodAdvancer.h`

**Type:** `struct` (static utility, no UObject overhead, mirrors `FWolfPresageSimulator` pattern)

```cpp
struct WOLFCORE_API FAbilityPeriodAdvancer
{
    /**
     * Advances the period of an active combat ability by the given elapsed time.
     *
     * Evaluates whether the current period's duration has been exceeded,
     * advances the period index if so, and resets the period timer.
     * Returns the new period timer value for the caller to store.
     *
     * @param ActiveAbility  The currently active combat ability. Must be valid.
     * @param SimPeriodTime  Accumulated time elapsed in the current period (seconds).
     * @param MoveCompSpeed  MaxWalkSpeed from the character's movement component.
     *                       Used only when the current period is EPeriodType::MoveTo.
     * @param MoveCompAccel  MaxAcceleration from the movement component.
     * @param CurrentSpeed   Current velocity magnitude of the simulated character.
     * @param SimLocation    Current simulated world-space location of the character.
     * @return               Updated SimPeriodTime. Caller must write this back to
     *                       UWolfPresageComponent::SimPeriodTime.
     */
    static float AdvancePeriod(
        UBaseCombatAbility* ActiveAbility,
        float SimPeriodTime,
        float MoveCompSpeed,
        float MoveCompAccel,
        float CurrentSpeed,
        const FVector& SimLocation
    );
};
```

### Behaviour Contract

- If `ActiveAbility` is null or has no valid sequence at the current index, returns `SimPeriodTime` unchanged.
- If the current period duration has **not** been exceeded, returns `SimPeriodTime` unchanged.
- If duration **has** been exceeded:
  - Calls `ActiveAbility->SetCurrentPeriodIndex(CurrentIndex + 1)`.
  - Returns `0.f` (reset).
- `MoveTo` duration is computed via `ActiveAbility->CalculateMovementDuration(...)` using the passed movement parameters.
- All other period types use `ActiveAbility->GetPeriodDuration(CurrentPeriod)`.
- Logging (period completed, next index, sequence finished) remains here inside `FAbilityPeriodAdvancer::AdvancePeriod`, not in `SimulateTick`.

---

## Changes to `UBaseCombatAbility`

No new public API is required. `FAbilityPeriodAdvancer` calls the existing `GetCurrentPeriodIndex()`, `SetCurrentPeriodIndex()`, `GetPeriodDuration()`, and `CalculateMovementDuration()`. If any of these are currently non-`const` where they could be, they should be made `const`.

`AbilitySequence` **must not** be accessed directly by the advancer via a public field. If it is currently `public`, add a getter:

```cpp
// UBaseCombatAbility — new or promoted accessor
const TArray<FAbilityPeriod>& GetAbilitySequence() const { return AbilitySequence; }
```

`AbilitySequence` itself should then become `protected` or `private`.

---

## Changes to `UWolfPresageComponent::SimulateTick()`

Remove the entire offending block. Replace with:

```cpp
// After SimulateAnimationStep(Step):
if (auto* ActiveAbility = CharacterOwner->GetActiveCombatAbility())
{
    SimPeriodTime = FAbilityPeriodAdvancer::AdvancePeriod(
        ActiveAbility,
        SimPeriodTime,
        GetMoveComp()->MaxWalkSpeed,
        GetMoveComp()->MaxAcceleration,
        GetMoveComp()->Velocity.Size(),
        SimulationTransform.GetLocation()
    );
}

// Snapshot population — unchanged, but now unconditional:
FutureFrame.ActiveAbility = CharacterOwner->GetActiveCombatAbility();
FutureFrame.CurrentPeriodIndex = FutureFrame.ActiveAbility.IsValid()
    ? FutureFrame.ActiveAbility->GetCurrentPeriodIndex()
    : -1;
```

`SimulateTick` retains no knowledge of `AbilitySequence`, `EPeriodType`, or period durations.

---

## `SimPeriodTime` Ownership

`SimPeriodTime` is currently a `public` field on `UWolfPresageComponent`. It is written by:

- `FWolfPresageSimulator::SetupCombatantSimulation()` (sync from active ability progress)
- `UWolfPresageComponent::SimulateTick()` (increment and reset)

After this extraction it is still written in both places, but the reset path moves inside `FAbilityPeriodAdvancer::AdvancePeriod` — the return value is written back by `SimulateTick`. No change to `SimPeriodTime` visibility is strictly required, but it should be made `private` with a setter to prevent external mutation:

```cpp
// UWolfPresageComponent
public:
    void SetSimPeriodTime(float Value) { SimPeriodTime = Value; }
    float GetSimPeriodTime() const { return SimPeriodTime; }

private:
    float SimPeriodTime = 0.f;
```

`FWolfPresageSimulator::SetupCombatantSimulation()` is updated to call `Presage->SetSimPeriodTime(CurrentProgress)`.

---

## Files Affected

| File | Change |
|---|---|
| `Abilities/AbilityPeriodAdvancer.h` | **New** — struct declaration |
| `Abilities/AbilityPeriodAdvancer.cpp` | **New** — `AdvancePeriod` implementation |
| `Core/WolfPresageComponent.h` | `SimPeriodTime` becomes `private`; add getter/setter |
| `Core/WolfPresageComponent.cpp` | Remove offending block; call `FAbilityPeriodAdvancer::AdvancePeriod` |
| `Core/WolfPresageSimulator.cpp` | Update `SetupCombatantSimulation` to use `SetSimPeriodTime()` |
| `Abilities/BaseCombatAbility.h` | Add `GetAbilitySequence()` const accessor; make `AbilitySequence` protected |

---

## What Does Not Change

- `FWolfPresageSimulator::ExecuteFutureBake()` — orchestration loop is unaffected.
- `UWolfSnapshotComponent` — snapshot capture of `ActiveAbility` and `CurrentPeriodIndex` is unaffected.
- `AWolfCharacterBase::GetActiveAbilityProgress()` — unaffected.
- All existing unit/integration test surface for presage baking remains the same from the outside.

---

## Acceptance Criteria

1. `UWolfPresageComponent` contains no direct access to `AbilitySequence`, `EPeriodType`, or period duration calculation.
2. `FAbilityPeriodAdvancer::AdvancePeriod` is the single site responsible for period index mutation during simulation.
3. `SimPeriodTime` is not directly writable from outside `UWolfPresageComponent`.
4. Simulation bake output (prediction buffer contents) is identical before and after the refactor for identical inputs.
5. No new coupling is introduced between `FAbilityPeriodAdvancer` and `UWolfPresageComponent` — the advancer takes only primitives and a pointer to the ability.
