# Spec: Data-Driven Hit Effects for Combat Abilities

**Status:** Ready for implementation
**Affected files:** `BaseCombatAbility.h/.cpp`, `RTCombatAbility.h/.cpp`, `TBCombatAbility.h/.cpp` (hook only)

---

## 1. Summary

Replace the fixed `FlowGain` / `AdrenalineGain` / `Damage` fields on `FCombatPeriod` (and the
matching `FlowGainEffect` / `AdrenalineGainEffect` / `DamageEffect` class refs on
`UBaseCombatAbility`) with a single Blueprint-extensible container:

```cpp
TArray<FCombatHitEffect> HitEffects;
```

Each `FCombatHitEffect` bundles a `GameplayEffect` class, a magnitude, and a target selector
(self vs. target). This makes the set of attribute effects per period open-ended instead of
hardcoded to three, and moves effect *application* into a shared virtual on the base class so
`UTBCombatAbility` can reuse it instead of re-deriving `URTCombatAbility`'s logic later.

---

## 2. Motivation

Today:
- `FCombatPeriod` has exactly three attribute channels: Flow, Adrenaline, Damage.
- The `GameplayEffect` classes for those three channels live on `UBaseCombatAbility` as fixed
  members (`FlowGainEffect`, `AdrenalineGainEffect`, `DamageEffect`), one per ability instance,
  not per period.
- Only `URTCombatAbility::ApplyCombatEffect` knows how to build a spec and apply it
  (self or target, hardcoded per call site in `ProcessAttackHit`).
- `UTBCombatAbility` has no equivalent and will need one.

This doesn't scale if we want a fourth attribute, a period that grants an effect to self *and*
target, or different effect classes per period (e.g. a heavy attack period using a different
damage GE than a light one).

---

## 3. Goals

- Arbitrary number of hit effects per `FCombatPeriod`, each independently targeted (self/target).
- Fully Blueprint-authorable (designers add rows in the editor, no C++ changes to add an effect).
- Single shared application path usable by both `URTCombatAbility` (now) and
  `UTBCombatAbility` (future), living on `UBaseCombatAbility`.
- No change to the *timing/detection* logic (trace, montage notify, hit delay) — this spec is
  scoped to *what happens once a hit/target is resolved*, not *how* a hit is detected.

## 4. Non-Goals

- Not implementing `UTBCombatAbility`'s actual attack detection (no trace, no target
  resolution) — only providing the hook it will call once that lands.
- Not changing `EPeriodType`, montage/notify handling, or movement periods.
- Not adding effect stacking/removal semantics, only application.

---

## 5. Proposed Design

### 5.1 New struct: `FCombatHitEffect`

Declared in `BaseCombatAbility.h`, alongside `FCombatPeriod`.

```cpp
/**
 * Describes a single GameplayEffect application tied to a combat period — the effect class,
 * its magnitude, and whether it targets the source (self) or the target actor.
 */
USTRUCT(BlueprintType)
struct FCombatHitEffect
{
    GENERATED_BODY()

    /** GameplayEffect to apply. If unset, this entry is skipped. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WolfCore|Hit Effects")
    TSubclassOf<UGameplayEffect> EffectClass;

    /** Magnitude passed via SetByCaller. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WolfCore|Hit Effects")
    FScalableFloat Amount = 0.f;

    /** True = apply to the source (attacking) actor. False = apply to the target actor. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WolfCore|Hit Effects")
    bool bSelfTarget = false;

    /**
     * SetByCaller tag for the magnitude. Left invalid by default; UBaseCombatAbility::ApplyHitEffects
     * falls back to FWolfGameplayTags::Get().Data_Amount when this is unset, matching current
     * behavior where every effect used the same tag. Can be overridden per-entry if a future GE
     * needs a distinct SetByCaller tag.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WolfCore|Hit Effects")
    FGameplayTag DataAmountTag;
};
```

### 5.2 `FCombatPeriod` changes

**Remove:**
```cpp
FScalableFloat FlowGain = 0.f;
FScalableFloat AdrenalineGain = 0.f;
FScalableFloat Damage = 0.f;
```

**Add:**
```cpp
/** All GameplayEffects applied when this period's hit resolves (damage, resource gain, etc). */
UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WolfCore|Attribute Effects")
TArray<FCombatHitEffect> HitEffects;
```

### 5.3 `UBaseCombatAbility` changes

**Remove** the now-redundant fixed effect class members:
```cpp
TSubclassOf<UGameplayEffect> FlowGainEffect;
TSubclassOf<UGameplayEffect> AdrenalineGainEffect;
TSubclassOf<UGameplayEffect> DamageEffect;
```

**Add** a shared, virtual application function:

```cpp
/**
 * Applies every FCombatHitEffect entry in Period between this ability's source actor and
 * TargetActor. Central extension point so derived abilities (RT trace-based, TB
 * prediction-based) share one application path. Virtual so a derived ability can override
 * application semantics if it ever needs to (e.g. different mitigation rules) without touching
 * callers.
 *
 * @param Period      The period whose HitEffects should be applied.
 * @param TargetActor The resolved target actor (RT: trace hit actor; TB: blackboard target).
 * @param HitResult    Optional trace hit result, added to the effect context if present.
 * @return True if at least one effect was applied. Kept as bool since RT's debug draw and
 *         potential future callers can use it; if it ends up unused by every caller, this can
 *         be simplified to void later.
 */
virtual bool ApplyHitEffects(const FCombatPeriod& Period, AActor* TargetActor, const FHitResult* HitResult = nullptr);
```

Implementation (`BaseCombatAbility.cpp`):

```cpp
bool UBaseCombatAbility::ApplyHitEffects(const FCombatPeriod& Period, AActor* TargetActor, const FHitResult* HitResult)
{
    if (!TargetActor) return false;

    auto* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!SourceASC) return false;

    auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (!TargetASC) return false;

    auto EffectContext = SourceASC->MakeEffectContext();
    if (HitResult) EffectContext.AddHitResult(*HitResult);

    const auto AbilityLevel = GetAbilityLevel();
    bool bAppliedAny = false;

    for (const FCombatHitEffect& Effect : Period.HitEffects)
    {
        if (!Effect.EffectClass) continue;

        const auto EffectSpecHandle = SourceASC->MakeOutgoingSpec(Effect.EffectClass, AbilityLevel, EffectContext);
        if (!EffectSpecHandle.IsValid()) continue;

        const FGameplayTag AmountTag = Effect.DataAmountTag.IsValid()
            ? Effect.DataAmountTag
            : FWolfGameplayTags::Get().Data_Amount;

        EffectSpecHandle.Data->SetSetByCallerMagnitude(AmountTag, Effect.Amount.GetValueAtLevel(AbilityLevel));

        if (Effect.bSelfTarget) SourceASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
        else                    SourceASC->ApplyGameplayEffectSpecToTarget(*EffectSpecHandle.Data.Get(), TargetASC);

        bAppliedAny = true;
    }

    return bAppliedAny;
}
```

`#include "AbilitySystemBlueprintLibrary.h"` needs to be added to `BaseCombatAbility.cpp`
(`Core/WolfGameplayTags.h` is already included there).

### 5.4 `URTCombatAbility` changes

`ProcessAttackHit` no longer builds specs — it only validates the hit (still a trace-specific
concern: cast to `AWolfCharacterBase`, reject self-hits, confirm target has an ASC) and then
delegates:

```cpp
bool URTCombatAbility::ProcessAttackHit(const FHitResult& Hit, const FCombatPeriod& ContextPeriod, const AActor* Avatar)
{
    auto* TargetChar = Cast<AWolfCharacterBase>(Hit.GetActor());
    if (!TargetChar || TargetChar == Avatar) return false;

    if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetChar)) return false;

    WOLF_INFO("Hit character: %s", *TargetChar->GetName());
    ApplyHitEffects(ContextPeriod, TargetChar, &Hit);

    return true; // target validity, independent of whether any effect actually applied
}
```

Note the return value is deliberately kept as "did we find a valid enemy" (drives the debug
cylinder color), not "did an effect apply" — that matches current behavior where the cylinder
turns green on any valid hit, even in the hypothetical case where a period has no effects.

Confirmed: `ProcessAttackHit` is only ever called from `HandleAttackHitEvent`, so this signature
change is safe — no other call sites to update.

**Removed entirely from RT:**
- `ApplyCombatEffect` (static method) — logic now lives in `UBaseCombatAbility::ApplyHitEffects`.
- The `MyASC` / `AbilityLevel` fetch-and-pass-through in `HandleAttackHitEvent` and
  `ProcessAttackHit`'s signature — no longer needed since `ApplyHitEffects` resolves those itself.

`HandleAttackHitEvent` simplifies to just: trace, then for each hit call `ProcessAttackHit`,
tracking `bValidTargetFound` for the debug draw — the ASC/ability-level bookkeeping goes away.

### 5.5 `UTBCombatAbility` (future hook, not implemented now)

When TB's attack resolution lands, its override of `HandleAttackHitEvent` should:
1. Resolve its target (e.g. via `GetTargetFromBlackboard()`, already on the base class).
2. Call `ApplyHitEffects(CurrentAttackPeriod, ResolvedTarget)` — passing `nullptr` for
   `HitResult` since there's no trace.

No changes needed to the base class beyond what's specified in §5.3 to support this — that's
the point of putting it there now.

---

## 6. Removed / Deprecated Members

| Member | Was on | Fate |
|---|---|---|
| `FCombatPeriod::FlowGain` | `BaseCombatAbility.h` | Removed, replaced by `HitEffects` entries |
| `FCombatPeriod::AdrenalineGain` | `BaseCombatAbility.h` | Removed, replaced by `HitEffects` entries |
| `FCombatPeriod::Damage` | `BaseCombatAbility.h` | Removed, replaced by `HitEffects` entries |
| `UBaseCombatAbility::FlowGainEffect` | `BaseCombatAbility.h` | Removed, class now lives per-entry |
| `UBaseCombatAbility::AdrenalineGainEffect` | `BaseCombatAbility.h` | Removed, class now lives per-entry |
| `UBaseCombatAbility::DamageEffect` | `BaseCombatAbility.h` | Removed, class now lives per-entry |
| `URTCombatAbility::ApplyCombatEffect` | `RTCombatAbility.h/.cpp` | Removed, folded into `UBaseCombatAbility::ApplyHitEffects` |

---

## 7. Edge Cases & Behavior Notes

- **Multiple targets per swing:** RT's sphere trace can hit N enemies in one attack. Each hit
  runs the full `HitEffects` loop independently, so a `bSelfTarget = true` entry (e.g. Flow gain)
  is applied once per enemy struck, not once per swing — this matches current behavior and is
  called out here so it's a conscious carry-over, not a silent regression.
- **Missing `EffectClass`:** entry is silently skipped (matches current `ApplyCombatEffect`
  behavior for a null class).
- **No valid target:** `ApplyHitEffects` bails early and returns `false`; RT's target-validity
  check (cast + self-hit rejection) happens before this is even called.
- **Empty `HitEffects` array:** valid — a period with no attribute effects (e.g. a pure windup
  or evasion period) just does nothing when `ApplyHitEffects` is called, no special-casing needed
  since `Attack`-type periods are the only ones that call it and non-attack periods never do.

---

## 8. Resolved Design Decisions

These were open questions in the initial draft; recording the answers here for reference.

1. **`DataAmountTag` override:** kept. Defaults to `FWolfGameplayTags::Get().Data_Amount` via the
   fallback in `ApplyHitEffects` (see §5.3) when left unset on the struct.
2. **Struct name:** `FCombatHitEffect` (was `FHitEffect` in the initial draft).
3. **Return type of `ApplyHitEffects`:** `bool`. See the comment on the function in §5.3 — if it
   turns out no caller ever uses the return value, this is a candidate to simplify to `void`.
4. **`ProcessAttackHit` signature change:** confirmed safe — it has no callers outside
   `HandleAttackHitEvent`.

---

## 9. Testing Plan

- Re-author one existing RT ability's periods with equivalent `HitEffects` rows and confirm
  damage/Flow/Adrenaline application matches pre-refactor behavior (same GEs, same magnitudes).
- Multi-target trace hit: confirm self-target effects (Flow/Adrenaline) apply once per enemy
  struck, as before.
- Empty `HitEffects` array on a period: confirm no crash, no effect application.
- Null `EffectClass` in one row alongside valid rows: confirm the valid rows still apply and the
  null row is skipped without affecting the others.
- Debug cylinder still turns green/red based on target validity, independent of `HitEffects`
  contents.

---

## 10. Rollout Steps

1. Land `FCombatHitEffect` struct + `FCombatPeriod` field swap + `UBaseCombatAbility::ApplyHitEffects`.
2. Update `URTCombatAbility` to the simplified `ProcessAttackHit`/`HandleAttackHitEvent`, remove
   `ApplyCombatEffect`.
3. Re-author existing ability content to use `HitEffects` rows in place of the old fields.
4. Merge, verify in-editor that RT abilities behave identically to before.
5. `UTBCombatAbility` attack-resolution work (separate task) calls `ApplyHitEffects` per §5.5 —
   no further base-class changes anticipated.
