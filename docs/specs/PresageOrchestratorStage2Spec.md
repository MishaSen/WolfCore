# PresageOrchestratorStage2Spec.md

## Presage Orchestrator — Stage 2: Prerequisite Ability-Injection Fixes

**Status:** Ready for implementation
**Scope:** Two targeted bug fixes in the existing player-ability-injection code. No new types, no orchestrator logic. This stage exists because stage 3 (planning/execution wiring) will build directly on top of `TryActivateInjectedAbility`/`GetActiveSimulationAbility`, and both currently have a latent bug that stage 3 would otherwise inherit.
**Depends on:** `PresageOrchestratorSpec.md` section 4.3 (names both fixes as prerequisites)
**Touches:** `WolfPresageComponent.h`, `WolfPresageComponent.cpp`

This is stage 2 of 5 in the orchestrator rollout. Do not touch `PresageOrchestratorTypes.h`, `ComputeAbilityTiming`, `GetOrComputeTimingProfile`, or anything from stage 1 in this pass — those are done and unrelated to this stage's two fixes. Do not add planning/execution wiring — that's stage 3.

---

## Fix 1: `InjectedAbilityRequest` is never cleared

### Current behavior

```cpp
void UWolfPresageComponent::BeginSimulation()
{
	SimElapsedTime = 0.f;
	SimPeriodTime = 0.f;
	bSimulatedAbilityActive = false;
	LastSimulatedPeriodIndex = -1;
	SimulatedAbility = nullptr;
}

void UWolfPresageComponent::EndSimulation()
{
	SimulatedAbility = nullptr;
	bSimulatedAbilityActive = false;
	LastSimulatedPeriodIndex = -1;
}
```

`ClearInjectedAbilityRequest()` exists (`InjectedAbilityRequest = FPresageAbilityRequest();`) but is never called from `BeginSimulation()`, `EndSimulation()`, or anywhere else. Once a player injects an ability, `InjectedAbilityRequest` stays populated indefinitely — surviving past the end of the bake it was meant for, past leaving TB, and into every future TB session — so `TryActivateInjectedAbility()` will silently re-fire that same stale request at the start of every subsequent bake until a new request happens to overwrite it.

### Fix

Add the call to `EndSimulation()`, since that's the one place that already runs exactly once per bake for every combatant, marking "this bake is done, this combatant's per-bake state resets":

```cpp
void UWolfPresageComponent::EndSimulation()
{
	SimulatedAbility = nullptr;
	bSimulatedAbilityActive = false;
	LastSimulatedPeriodIndex = -1;
	ClearInjectedAbilityRequest();
}
```

Do not add this to `BeginSimulation()` instead — `BeginSimulation()` runs at the *start* of a bake, meaning a request set by `TryInjectPresageAbility()` immediately before the bake starts (which is the normal flow — see `ReBakeTimeline`) would be cleared before `TryActivateInjectedAbility()` ever gets to read it. `EndSimulation()` is the correct place because it runs after the bake that was supposed to consume the request has finished consuming it.

---

## Fix 2: `GetActiveSimulationAbility()` permanently blocks the injected ability once a real ability's sequence is exhausted

### Current behavior

```cpp
UBaseCombatAbility* UWolfPresageComponent::GetActiveSimulationAbility() const
{
	if (!CharacterOwner) return nullptr;

	if (auto* RealAbility = CharacterOwner->GetActiveCombatAbility())
	{
		return RealAbility;
	}

	if (bSimulatedAbilityActive && SimulatedAbility)
	{
		return SimulatedAbility;
	}

	return nullptr;
}
```

`CharacterOwner->GetActiveCombatAbility()` reflects real GAS `IsActive()` bookkeeping (see `AWolfCharacterBase::GetActiveCombatAbility()`/`CachedActiveAbility`) — not whether that ability's simulated period sequence has anywhere left to go during this bake. If a real ability is active when TB starts (the normal case of switching modes mid-attack) and its sequence runs out partway through the bake, `RealAbility->GetCurrentPeriodIndex()` becomes an out-of-bounds index and `FAbilityPeriodAdvancer::AdvancePeriod` correctly becomes a no-op on it — but `GetActiveSimulationAbility()` has no way to know that, and keeps returning that same now-inert real ability for the rest of the bake. Any ability injected after that point gets created (`TryActivateInjectedAbility` still runs, `bSimulatedAbilityActive` becomes true, the montage syncs once at injection) but never actually drives period-advancement or velocity, because every tick's `GetActiveSimulationAbility()` call keeps handing back the exhausted real ability instead.

### Fix

Check whether the real ability's sequence still has periods left before preferring it. `UBaseCombatAbility` already exposes both `GetCurrentPeriodIndex()` and `GetAbilitySequence()`, so this needs no new API — just a validity check at the existing read site:

```cpp
UBaseCombatAbility* UWolfPresageComponent::GetActiveSimulationAbility() const
{
	if (!CharacterOwner) return nullptr;

	if (auto* RealAbility = CharacterOwner->GetActiveCombatAbility())
	{
		if (RealAbility->GetAbilitySequence().IsValidIndex(RealAbility->GetCurrentPeriodIndex()))
		{
			return RealAbility;
		}
		// RealAbility's simulated sequence has run its course for this bake — fall through so an
		// injected ability (if any) can take over instead of being permanently blocked.
	}

	if (bSimulatedAbilityActive && SimulatedAbility)
	{
		return SimulatedAbility;
	}

	return nullptr;
}
```

Note this only changes what the *simulation* treats as "driving" the combatant this tick — it does not touch the real ability's actual GAS state (`IsActive()`, `CachedActiveAbility`) in any way. That's real-time bookkeeping, untouched by this fix, and correctly stays real until the real ability genuinely ends in real time later.

---

## Non-Goals for This Stage

- Do not change `TryActivateInjectedAbility()` itself — the gating logic there (`bSimulatedAbilityActive`, `HasInjectedAbilityRequest()`, `SimElapsedTime < ScheduledTime`) is unaffected by either fix and is out of scope.
- Do not address `ScheduledTime` always being `0.f` (`BuildInitialPresageRequest` in `WolfAbilitySystemComponent.cpp`) — that's a separate, already-flagged gap, not part of either bug this stage fixes, and not required for stage 3.
- Do not touch `GatherPresageTargets` or any targeting logic.
- Do not add any new logging beyond what's needed to verify the fixes (see Testing below) unless it matches existing logging conventions already in these two functions.

---

## Testing / Validation

1. **Request no longer leaks across bakes.** Inject an ability, let the bake complete, exit TB, re-enter TB in a fresh session without injecting anything new — confirm the old ability does *not* silently re-fire. (Before the fix: it would.)
2. **Request still fires within its own bake.** Confirm the existing single-bake injection flow (inject → `ReBakeTimeline` → ability appears in the baked preview) still works exactly as before — this fix must not break the case it's meant to preserve.
3. **Exhausted real ability no longer blocks injection.** Construct a scenario where a real ability is active and mid-sequence when TB starts, let its sequence run out partway through the baked duration, then inject a new ability — confirm the injected ability's periods now actually advance (montage transitions past its first period, not just the initial sync) for the remainder of the bake.
4. **Still-active real ability still takes priority.** Construct a scenario where the real ability's sequence has *not* yet run out, and confirm `GetActiveSimulationAbility()` still correctly returns the real ability, not any injected one — this fix must only change behavior once the real ability's sequence is exhausted, not before.
