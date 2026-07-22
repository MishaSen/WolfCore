# RTCombatAbility Implementation Notes

This document describes the structure of `URTCombatAbility`'s attack-hit handling
after refactoring the hit-processing loop out of `HandleAttackHitEvent`.

## Overview

`URTCombatAbility` is a Gameplay Ability that performs a sphere trace in front of
the avatar, finds valid `AWolfCharacterBase` targets, and applies gameplay effects
(flow gain, adrenaline gain, damage) to each one. It also draws a debug cylinder
showing the swept trace volume and whether it connected with a target.

## Call Flow

```
ActivateAbility
  └─ StartCombatSequence
       └─ HandleAttackHitEvent(ContextPeriod)
            ├─ PerformAttackTrace(...)
            ├─ ProcessAttackHit(...)            \[per hit result]
            │     └─ ApplyCombatEffect(...)      \[per effect: flow / adrenaline / damage]
            └─ DrawAttackDebugCylinder(...)
```

## Functions

### `ActivateAbility`

Standard GAS entry point. Calls `Super::ActivateAbility` then kicks off
`StartCombatSequence`.

### `HandleAttackHitEvent(const FCombatPeriod\& ContextPeriod)`

Orchestrator. Responsibilities:

1. Resolve the avatar and ability system component, bailing early if either is null.
2. Compute the trace start/end points.
3. Delegate the actual trace to `PerformAttackTrace`.
4. Iterate hit results, delegating each to `ProcessAttackHit`.
5. Draw the debug cylinder based on whether any valid target was found.

This function intentionally contains no effect-application or tracing logic itself —
it reads as a short pipeline of named steps.

### `PerformAttackTrace(AActor\* Avatar, const FVector\& Start, const FVector\& End, TArray<FHitResult>\& OutHits)`

Wraps `UKismetSystemLibrary::SphereTraceMulti` using `AttackRadius`, ignoring the
avatar itself, with debug drawing disabled (handled separately by
`DrawAttackDebugCylinder`).

### `ProcessAttackHit(const FHitResult\& Hit, const FCombatPeriod\& ContextPeriod, AActor\* Avatar, UAbilitySystemComponent\* MyASC, float AbilityLevel)`

Per-hit-result logic, extracted from the original inline loop body:

1. Casts the hit actor to `AWolfCharacterBase`; skips if invalid or if it's the avatar.
2. Resolves the target's `UAbilitySystemComponent`; skips if none.
3. Builds an effect context with the hit result attached.
4. Applies the three combat effects via `ApplyCombatEffect`.
5. Returns `true` if a valid target was processed (used to drive debug color),
`false` otherwise.

### `ApplyCombatEffect(UAbilitySystemComponent\* SourceASC, UAbilitySystemComponent\* TargetASC, const FGameplayEffectContextHandle\& EffectContext, float AbilityLevel, const TSubclassOf<UGameplayEffect>\& EffectClass, const FScalableFloat\& Amount, bool bToTarget, FGameplayTag DataAmountTag = FWolfGameplayTags::Get().Data\_Amount)`

Replaces the original `\[\&]` lambda with a named, reusable member function:

1. No-ops if `EffectClass` is null.
2. Builds an outgoing spec at `AbilityLevel`.
3. Sets the SetByCaller magnitude for the given data tag.
4. Applies to either the target or self, depending on `bToTarget`.

### `DrawAttackDebugCylinder(AActor\* Avatar, const FVector\& StartPos, const FVector\& EndPos, float Radius, bool bValidTargetFound) const`

Draws a debug cylinder along the trace path — green if any valid target was hit,
red otherwise. Takes `Avatar` as a parameter rather than re-resolving it via
`GetAvatarActorFromActorInfo()`, since the caller already has it in scope.

## Rationale for the Refactor

* **Single Responsibility**: `HandleAttackHitEvent` previously mixed tracing,
iteration, effect application, and debug drawing in one function. Splitting
these into named functions makes each piece independently readable and testable.
* **Named lambda → member function**: `ApplyEffect` was a `\[\&]`-capturing lambda
defined inside the loop. Since it didn't need to capture ambient state (everything
it used could be passed as a parameter), promoting it to `ApplyCombatEffect`
removes a layer of implicit capture and makes it reusable outside this context.
* **Avoiding redundant work**: `GetAvatarActorFromActorInfo()` was called twice
(once in the handler, once again inside the debug-draw function). Passing
`Avatar` through as a parameter avoids the duplicate lookup.
* **Readability at the call site**: After extraction, `HandleAttackHitEvent` reads
almost like pseudocode — trace, process hits, draw debug — with implementation
details pushed down a level.

