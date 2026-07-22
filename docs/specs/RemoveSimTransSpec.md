# Spec: Remove SimulationTransform — Use Actor Directly During Presage Baking

## Problem

Both `UWolfPresageComponent` and `UWolfSnapshotComponent` each maintain their own `FTransform SimulationTransform`, guarded by a `bIsSimulating` flag, with `GetSimLocation()` / `GetSimRotation()` branching on that flag at every call site.

This creates several failure modes that are currently live:

1. **Sync drift.** `PresageComponent::SimulationTransform` and `SnapshotComponent::SimulationTransform` are separate values. `SetupCombatantSimulation` seeds the presage one, but the snapshot one is only synced *inside* `SimulateTick` — meaning the very first `CreateSnapshot` call in step 0 reads the snapshot component's stale transform (the live actor location from `BeginPlay`), not the seeded one.

2. **AI re-enters movement.** `StopMovementImmediately` only zeroes velocity for one frame. The `PathFollowingComponent` restarts movement on its next tick. Because the simulation runs synchronously in the same frame, the actual actor drifts while baking, making the starting position of snapshot[0] incorrect relative to what `GetSimulatedVelocity` expects.

3. **`AdvancePeriod` collapses early.** It computes remaining `MoveTo` duration from `FVector::Distance(SimLocation, Destination)`. When the actor has already drifted closer to the destination (because AI kept moving), that distance is small, so the period completes in 1–2 steps instead of the correct ~7. Everything past that point snaps to the post-`MoveTo` location — confirmed in the logs.

4. **Dead code accumulates.** `GetSimLocation()` / `GetSimRotation()` exist on both components purely to serve `bIsSimulating`. Snapshot creation doesn't actually need to know whether it's simulating — it just needs the correct location to write.

## Proposed Solution

**Move the actual actor during the bake. Restore it from `MasterStartSnapshot` when the bake is done.**

The `MasterStartSnapshot` is already captured in `UCombatModeSubsystem::SetMode` before `ExecuteFutureBake` is called. That snapshot is the ground truth for where every actor started. The bake can freely teleport actors, accumulate snapshots, and then `ScrubTimeline(0.f)` at the end of `SetMode` already restores them all to their initial state via `RestoreSnapshot`.

This means:
- `SimulationTransform` is deleted from both components.
- `bIsSimulating` is deleted from both components.
- `GetSimLocation()` / `GetSimRotation()` are deleted from both components.
- `SnapshotPhysics` always reads `GetOwnerLocation()` / `GetOwnerRotation()` (the live actor).
- `SimulatePhysicsStep` teleports the actor directly via `SetActorLocation`.
- `SetupCombatantSimulation` disables the `MoveComponent` tick and `PathFollowingComponent` tick to freeze AI movement for the duration of the bake.
- `CleanupCombatantSimulation` re-enables both ticks. The subsequent `ScrubTimeline(0.f)` call in `SetMode` restores the actor to its true pre-bake position, velocity, and ability state.

---

## Detailed Changes

### `WolfPresageComponent.h` / `.cpp`

**Remove:**
- `bool bIsSimulating`
- `FTransform SimulationTransform`
- `void SetIsSimulating(bool bState)`
- `void SetSimulationTransform(const FTransform& NewTransform)`
- `FVector GetSimLocation() const`
- `FRotator GetSimRotation() const`

**Modify `SimulateTick`:**
- Remove the seed line `if (PredictionBuffer.Num() == 0) SimulationTransform = CharacterOwner->GetActorTransform();` — the actor is already at the right location.
- Remove `SnapshotControl->SetIsSimulating(true/false)` calls.
- Remove `SnapshotControl->SetSimulationTransform(SimulationTransform)` call.
- The `CreateSnapshot` call becomes unconditional and reads the live actor directly.

**Modify `SimulatePhysicsStep`:**
- Replace `SimulationTransform.SetLocation(...)` calls with `CharacterOwner->SetActorLocation(..., ETeleportType::TeleportPhysics)`.
- Replace `SimulationTransform.GetLocation()` reads with `CharacterOwner->GetActorLocation()`.
- Replace `SimulationTransform.GetRotation()` with `CharacterOwner->GetActorQuat()`.

**Modify `GetSimulatedVelocity`:**
- Replace `SimulationTransform.GetLocation()` with `CharacterOwner->GetActorLocation()`.
- Remove the `!bIsSimulating` early-out guard — the function is only called during baking, so it's always in a simulating context.

**Modify `SimulateAnimationStep`:**
- Replace `SimulationTransform.AddToTranslation(WorldDelta)` with `CharacterOwner->AddActorWorldOffset(WorldDelta, false, nullptr, ETeleportType::TeleportPhysics)`.
- Remove the `bIsSimulating` guard and warning.

**Modify `AdvancePeriod` call site in `SimulateTick`:**
- Pass `CharacterOwner->GetActorLocation()` instead of `SimulationTransform.GetLocation()`.

---

### `WolfSnapshotComponent.h` / `.cpp`

**Remove:**
- `bool bIsSimulating`
- `FTransform SimulationTransform`
- `void SetIsSimulating(bool bState)`
- `void SetSimulationTransform(const FTransform& NewTransform)`
- `FVector GetSimLocation() const`
- `FRotator GetSimRotation() const`

**Modify `SnapshotPhysics`:**
- Replace `GetSimLocation()` with `GetOwnerLocation()`.
- Replace `GetSimRotation()` with `GetOwnerRotation()`.
- These now unconditionally read the live actor, which is correct because the actor is being directly moved during baking.

---

### `WolfPresageSimulator.cpp`

**Modify `SetupCombatantSimulation`:**

```cpp
void FWolfPresageSimulator::SetupCombatantSimulation(
    const TScriptInterface<IWolfCombatant>& Combatant, float Duration)
{
    auto* Actor = Cast<AActor>(Combatant.GetObject());
    if (!IsValid(Actor)) return;

    auto* Presage = Combatant.GetInterface()->GetPresageComponent();
    if (!IsValid(Presage)) return;

    Presage->ClearPredictionBuffer(Duration);

    // Sync sim period time. Use 0 so AdvancePeriod computes remaining
    // duration from the actor's current position, not elapsed time.
    Presage->SetSimPeriodTime(0.f);

    // Freeze movement so the AI cannot move the actor during the synchronous bake.
    if (auto* MoveComp = Actor->FindComponentByClass<UCharacterMovementComponent>())
    {
        MoveComp->StopMovementImmediately();
        MoveComp->SetComponentTickEnabled(false);
    }

    if (const auto* Pawn = Cast<APawn>(Actor))
    {
        if (auto* AIC = Cast<AAIController>(Pawn->GetController()))
        {
            if (auto* PFC = AIC->GetPathFollowingComponent())
            {
                PFC->SetComponentTickEnabled(false);
            }
        }
    }
}
```

**Remove** `Presage->SetIsSimulating(true)` and `Presage->SetSimulationTransform(...)` — no longer needed.

**Modify `CleanupCombatantSimulation`:**

```cpp
void FWolfPresageSimulator::CleanupCombatantSimulation(
    const TScriptInterface<IWolfCombatant>& Combatant)
{
    auto* Actor = Cast<AActor>(Combatant.GetObject());
    if (!IsValid(Actor)) return;

    // Re-enable movement ticks. The subsequent ScrubTimeline(0.f) in SetMode
    // will restore the actor to its pre-bake position via MasterStartSnapshot.
    if (auto* MoveComp = Actor->FindComponentByClass<UCharacterMovementComponent>())
    {
        MoveComp->SetComponentTickEnabled(true);
    }

    if (const auto* Pawn = Cast<APawn>(Actor))
    {
        if (auto* AIC = Cast<AAIController>(Pawn->GetController()))
        {
            if (auto* PFC = AIC->GetPathFollowingComponent())
            {
                PFC->SetComponentTickEnabled(true);
            }
        }
    }
}
```

**Remove** `Presage->SetIsSimulating(false)` — no longer needed.

---

### `WolfPresageSimulator.h`

No interface changes needed. The public API (`ExecuteFutureBake`, `ApplyPresageDrainEffect`) is unchanged.

---

### `WolfCharacterBase` / `CombatModeSubsystem`

No changes needed. The `SetMode` flow already calls `ExecuteFutureBake` then `ScrubTimeline(0.f)`, and `ScrubTimeline` calls `RestoreSnapshot` on every combatant using `MasterStartSnapshot`, which was captured before the bake began. This is the mechanism that restores actors to their pre-bake position.

---

## Why `SimPeriodTime` Starts at 0

Previously, `SetupCombatantSimulation` seeded `SimPeriodTime` with `GetActiveAbilityProgress()` (elapsed time since the period started). For `MoveTo`, `AdvancePeriod` compares this against the *distance-derived duration*. Starting with elapsed time causes a mismatch: the elapsed time can already exceed the remaining travel time, so the period completes on step 0.

Starting at `0.f` is correct because `AdvancePeriod` derives `MoveTo` duration from `FVector::Distance(ActorLocation, Destination)` — the actor's *current* position already encodes how far it has already traveled. The remaining distance naturally computes the correct remaining time without needing to offset by elapsed progress.

For non-`MoveTo` periods (fixed-duration `Wait`, `Attack`, etc.), starting at `0` is also correct: if an `Attack` montage is mid-play, the actor's montage position (read by `SimulateAnimationStep`) already represents the current playback state, and the fixed duration in `GetPeriodDuration` will produce the right step count.

---

## Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Teleporting the actor could trigger overlap/hit events mid-bake | Add `Actor->SetActorEnableCollision(false)` in setup and restore in cleanup, or use `ETeleportType::TeleportPhysics` which suppresses sweep events |
| AI Brain ticking between bake steps reads a mid-bake actor position | `MoveComponent` tick disabled prevents the AI from issuing move commands; behavior tree won't advance since its primary driver is the movement request, not position reads |
| `ScrubTimeline(0.f)` is currently gated by `FMath::IsNearlyEqual` which skips if `CurrentTimelineTime` is already 0 | In `SetMode`, `CurrentTimelineTime` is reset to `-1.f` (or any sentinel) before calling `ExecuteFutureBake`, so the scrub at `0.f` is never skipped. Alternatively, call `RestoreSnapshot` directly from `CleanupCombatantSimulation` using the pre-bake `MasterStartSnapshot`. |
| Root motion during baking applies to the live actor, affecting NavMesh queries | Root motion offsets are small per-step (`WolfSimConfig::Step` is likely ~0.1s). The actor is immediately restored after bake. Acceptable. |

---

## File Change Summary

| File | Changes |
|---|---|
| `WolfPresageComponent.h` | Remove `bIsSimulating`, `SimulationTransform`, `SetIsSimulating`, `SetSimulationTransform`, `GetSimLocation`, `GetSimRotation` |
| `WolfPresageComponent.cpp` | `SimulateTick`, `SimulatePhysicsStep`, `GetSimulatedVelocity`, `SimulateAnimationStep` — replace all `SimulationTransform` reads/writes with `CharacterOwner->GetActorLocation()` / `SetActorLocation()` |
| `WolfSnapshotComponent.h` | Remove `bIsSimulating`, `SimulationTransform`, `SetIsSimulating`, `SetSimulationTransform`, `GetSimLocation`, `GetSimRotation` |
| `WolfSnapshotComponent.cpp` | `SnapshotPhysics` — replace `GetSimLocation()` / `GetSimRotation()` with `GetOwnerLocation()` / `GetOwnerRotation()` |
| `WolfPresageSimulator.cpp` | `SetupCombatantSimulation` — remove `SetIsSimulating`, `SetSimulationTransform`, change `SetSimPeriodTime` to `0.f`, add MoveComp + PFC tick disable. `CleanupCombatantSimulation` — remove `SetIsSimulating`, add tick re-enable |
