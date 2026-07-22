# PresageOrchestratorStage1Spec.md

## Presage Orchestrator — Stage 1: Data Model Only

**Status:** Ready for implementation
**Scope:** New inert types and one pure function. **No behavior change.** Nothing added here is called from anywhere yet — this stage only needs to compile cleanly and be structurally correct.
**Depends on:** `PresageOrchestratorSpec.md` (sections 2.1–2.4), `PresageOrchestratorArchitecture.md`
**Touches:** `BaseCombatAbility.h`, `BaseCombatAbility.cpp`, `CombatModeSubsystem.h`, `WolfCombatSettings.h`, one new pair of files for the orchestrator's shared types

This is stage 1 of 5 in the orchestrator rollout (see `PresageOrchestratorSpec.md` section 9). Do not implement planning logic, execution wiring, or the prerequisite injection-bug fixes in this pass — those are stages 2–5. This pass is deliberately narrow: get the types on disk, correct, and compiling.

---

## 1. New file: `PresageOrchestratorTypes.h`

Create `Source/WolfCore/Public/Presage/PresageOrchestratorTypes.h`. This is a new header — none of these types have a natural existing home, and they'll be shared across `CombatModeSubsystem`, `BaseCombatAbility`, and the planning code added in later stages, so they shouldn't live inside any single one of those classes' headers.

```cpp
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Interfaces/IWolfCombatant.h"
#include "PresageOrchestratorTypes.generated.h"

class UBaseCombatAbility;
class UGameplayAbility;

/**
 * Derived timing breakdown of a combat ability's sequence, computed from its AbilitySequence
 * rather than separately authored (see UBaseCombatAbility::ComputeAbilityTiming). Used by the
 * Presage orchestrator's planning phase to reason about ability timing without running physics.
 */
USTRUCT(BlueprintType)
struct WOLFCORE_API FAbilityTimingProfile
{
	GENERATED_BODY()

	/** Time from ability start until the first hit-capable period begins. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float WindupDuration = 0.f;

	/** Time from ability start until the first hit-capable period begins (same value as WindupDuration; kept as a separate field for readability at call sites reasoning about window bounds rather than windup length). */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float ActiveWindowStart = 0.f;

	/** Time from ability start until the last hit-capable period ends. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float ActiveWindowEnd = 0.f;

	/** Time from the end of the active window until the ability's sequence fully completes. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float RecoveryDuration = 0.f;

	/** Total duration of the ability's full sequence. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float TotalDuration = 0.f;
};

/** Status of a single combatant's intent within a planning pass. See PresageOrchestratorSpec.md section 2.2. */
UENUM(BlueprintType)
enum class EIntentStatus : uint8
{
	Declared,
	Interrupted,
	Confirmed
};

/**
 * A single combatant's planned action for a portion of the baked timeline, produced by the
 * Presage orchestrator's planning phase. Not used or populated until stage 3 of the orchestrator
 * rollout — this stage only defines the type.
 */
USTRUCT(BlueprintType)
struct WOLFCORE_API FIntentEntry
{
	GENERATED_BODY()

	/** The combatant this intent belongs to. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TScriptInterface<IWolfCombatant> Combatant;

	/** The ability class this intent commits to using. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TSubclassOf<UBaseCombatAbility> AbilityClass;

	/** This ability's derived timing profile, cached here so planning doesn't recompute it. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	FAbilityTimingProfile Timing;

	/** The intended target of this ability, if any. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TWeakObjectPtr<AActor> Target;

	/** Approximate start time of this intent on the baked timeline, in seconds from bake start. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float StartTime = 0.f;

	/** Current resolution status of this intent. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	EIntentStatus Status = EIntentStatus::Declared;

	/** True if UnavailableUntil holds a meaningful value (e.g. this intent puts the combatant in
	  * hitstun until a known time). False means no such restriction is known for this entry. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	bool bHasUnavailableUntil = false;

	/** Only meaningful if bHasUnavailableUntil is true. Timeline time before which this combatant
	  * cannot be offered a new decision. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float UnavailableUntil = 0.f;
};

/**
 * A single weighted, tag-gated option for how a combatant can respond to having its planned
 * ability interrupted during the Presage orchestrator's planning phase. Authored per-ability
 * and/or as a project-wide default list (see UWolfCombatSettings::DefaultInterruptResponses).
 * Not read by any logic yet — this stage only defines the authoring surface.
 */
USTRUCT(BlueprintType)
struct WOLFCORE_API FInterruptResponseOption
{
	GENERATED_BODY()

	/** Identifies which response this is, e.g. Presage.Interrupt.Feint, .Dodge, .Parry, .TakeHit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Presage")
	FGameplayTag ResponseTag;

	/** Relative probability weight when multiple options are available and gated-in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Presage", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	/** This option is only available if the interrupted combatant/period currently has all of these tags. Leave empty for an always-available option. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Presage")
	FGameplayTagContainer RequiredTags;
};
```

Notes for the implementer:
- `WOLFCORE_API` on each struct matches the module's existing convention (see `FCombatHitEffect`/`FCombatPeriod` in `ActorSnapshot.h`, which don't use it since they're in the same module as their only consumers — but these new types are intended to be referenced from multiple modules' worth of future Blueprint content, so keep `WOLFCORE_API` for safety; if the project's convention is actually to omit it for intra-module structs, match whatever `FActorSnapshot` does instead of what's written above).
- `TWeakObjectPtr<AActor>` for `Target`, not a raw pointer — this struct may be held across a decision boundary where the target could conceivably become invalid; weak pointer is the safer default here even though it isn't strictly required by this stage alone.
- Do not add any constructors, helper functions, or logic beyond the plain data members and the one comment block per field. Section 3 below is the only function this stage introduces, and it lives on `UBaseCombatAbility`, not in this file.

---

## 2. `BaseCombatAbility.h` — add `ComputeAbilityTiming`

In the existing public section, directly below the existing `ResolveMoveToDestinations` declaration:

```cpp
	/** Resolves MoveTo destinations in a sequence using a source location and target actor. */
	static void ResolveMoveToDestinations(TArray<FCombatPeriod>& Sequence, const FVector& SourceLocation, AActor* Target);

	/**
	 * Computes a derived timing breakdown (windup / active window / recovery) for the given
	 * ability sequence. The active window is defined as the span from the first period containing
	 * at least one FCombatHitEffect through the last such period. Sequences with no hit-capable
	 * periods are treated as pure windup with no active window and no recovery.
	 * This is a pure function of the sequence data — it does not read any instance state and does
	 * not require a valid ability instance to call.
	 */
	static FAbilityTimingProfile ComputeAbilityTiming(const TArray<FCombatPeriod>& Sequence);
```

Add `#include "Presage/PresageOrchestratorTypes.h"` to `BaseCombatAbility.h`'s existing include block (needed for `FAbilityTimingProfile` in the return type).

---

## 3. `BaseCombatAbility.cpp` — implement `ComputeAbilityTiming`

Add directly below the existing `ResolveMoveToDestinations` implementation:

```cpp
FAbilityTimingProfile UBaseCombatAbility::ComputeAbilityTiming(const TArray<FCombatPeriod>& Sequence)
{
	FAbilityTimingProfile Profile;

	int32 FirstHitPeriod = INDEX_NONE;
	int32 LastHitPeriod = INDEX_NONE;
	float TotalDuration = 0.f;

	for (int32 i = 0; i < Sequence.Num(); ++i)
	{
		if (Sequence[i].HitEffects.Num() > 0)
		{
			if (FirstHitPeriod == INDEX_NONE)
			{
				FirstHitPeriod = i;
			}
			LastHitPeriod = i;
		}
		TotalDuration += Sequence[i].Duration;
	}

	Profile.TotalDuration = TotalDuration;

	if (FirstHitPeriod == INDEX_NONE)
	{
		// No hit-capable periods at all (e.g. a pure movement/buff ability) — treat the entire
		// sequence as windup, with no active window and no recovery.
		Profile.WindupDuration = TotalDuration;
		Profile.ActiveWindowStart = TotalDuration;
		Profile.ActiveWindowEnd = TotalDuration;
		Profile.RecoveryDuration = 0.f;
		return Profile;
	}

	float RunningTime = 0.f;
	for (int32 i = 0; i < FirstHitPeriod; ++i)
	{
		RunningTime += Sequence[i].Duration;
	}
	Profile.WindupDuration = RunningTime;
	Profile.ActiveWindowStart = RunningTime;

	for (int32 i = FirstHitPeriod; i <= LastHitPeriod; ++i)
	{
		RunningTime += Sequence[i].Duration;
	}
	Profile.ActiveWindowEnd = RunningTime;

	Profile.RecoveryDuration = Profile.TotalDuration - Profile.ActiveWindowEnd;
	return Profile;
}
```

Do not call this function from anywhere else in this stage (not from `ActivateAbility`, not from any Presage simulation code). It should exist, compile, and be callable, but nothing invokes it yet — that's stage 3's job.

---

## 4. `CombatModeSubsystem.h` — add the timing profile cache

In the private section, directly below the existing `BakedStepSize` member:

```cpp
	float BakedStepSize = 0.1f; // Fallback only; overwritten from UWolfCombatSettings in SetMode().

	/** Cache of derived ability timing profiles, keyed by ability class. AbilitySequence is
	  * authored per-class and doesn't vary per instance, so this is computed once per class on
	  * first request rather than recomputed for every planning decision. Not populated or read
	  * anywhere yet — added in this stage as inert storage; stage 3 (planning) is the first
	  * consumer. */
	UPROPERTY()
	TMap<TSubclassOf<UBaseCombatAbility>, FAbilityTimingProfile> TimingProfileCache;
```

Add a public accessor directly below the existing `GetBakedStepSize()`:

```cpp
	float GetBakedStepSize() const { return BakedStepSize; }

	/**
	 * Returns the cached timing profile for the given ability class, computing and caching it on
	 * first request via UBaseCombatAbility::ComputeAbilityTiming against the class's CDO sequence.
	 * Returns a default-constructed (all-zero) FAbilityTimingProfile if AbilityClass is null.
	 * Not called from anywhere yet in this stage.
	 */
	const FAbilityTimingProfile& GetOrComputeTimingProfile(TSubclassOf<UBaseCombatAbility> AbilityClass);
```

Add `#include "Presage/PresageOrchestratorTypes.h"` to `CombatModeSubsystem.h`'s existing include block. `UBaseCombatAbility` only needs a forward declaration (`class UBaseCombatAbility;`) in the header, since the member is a `TSubclassOf`/return-by-reference — add that forward declaration alongside the existing `class UWolfCombatant;` forward declaration if `UBaseCombatAbility.h` isn't already included there.

### `CombatModeSubsystem.cpp` — implement the accessor

```cpp
const FAbilityTimingProfile& UCombatModeSubsystem::GetOrComputeTimingProfile(TSubclassOf<UBaseCombatAbility> AbilityClass)
{
	static const FAbilityTimingProfile DefaultProfile;
	if (!AbilityClass)
	{
		return DefaultProfile;
	}

	if (const FAbilityTimingProfile* Existing = TimingProfileCache.Find(AbilityClass))
	{
		return *Existing;
	}

	const auto* CDO = AbilityClass->GetDefaultObject<UBaseCombatAbility>();
	const FAbilityTimingProfile Computed = CDO
		? UBaseCombatAbility::ComputeAbilityTiming(CDO->GetAbilitySequence())
		: FAbilityTimingProfile();

	return TimingProfileCache.Add(AbilityClass, Computed);
}
```

Add `#include "Combat/BaseCombatAbility.h"` to `CombatModeSubsystem.cpp` if not already present (needed for `GetDefaultObject<UBaseCombatAbility>()` and `GetAbilitySequence()`).

---

## 5. `WolfCombatSettings.h` — add the default interrupt response table

In the existing "Presage Configuration" section, directly below `PresageSimulationStep`:

```cpp
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Presage", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float PresageSimulationStep = 0.1f;

	/** Project-wide default interrupt response options, used when an ability doesn't specify its
	  * own list. Not read by any logic yet — the Presage orchestrator's interrupt resolution
	  * (added in a later stage) is the intended consumer. */
	UPROPERTY(Config, EditAnywhere, Category = "WolfCore|Presage")
	TArray<FInterruptResponseOption> DefaultInterruptResponses;
```

Add `#include "Presage/PresageOrchestratorTypes.h"` to `WolfCombatSettings.h`'s existing include block.

---

## 6. Non-Goals for This Stage

Do not, in this pass:
- Add a per-ability `TArray<FInterruptResponseOption>` override to `UBaseCombatAbility` — this stage only adds the project-wide default on `UWolfCombatSettings`. Per-ability override is a small addition but belongs with stage 4 (interrupt resolution), where it will actually be read.
- Add `FPresageOrchestrator` or any planning function — that's stage 3.
- Modify `ExecuteFutureBake`'s signature — that's stage 3/4.
- Fix `ClearInjectedAbilityRequest`/`GetActiveSimulationAbility` — that's stage 2, a separate pass.
- Call `ComputeAbilityTiming` or `GetOrComputeTimingProfile` from anywhere.

---

## 7. Acceptance Criteria

- Project compiles cleanly with no new warnings.
- `FAbilityTimingProfile`, `FIntentEntry`, `EIntentStatus`, `FInterruptResponseOption` exist in `PresageOrchestratorTypes.h` exactly as specified, all Blueprint-visible (`BlueprintType`/`BlueprintReadOnly` or `BlueprintReadWrite` as annotated above).
- `UBaseCombatAbility::ComputeAbilityTiming` is a `static` function callable without an instance, matches the derivation rules in section 3 (windup = time before first hit-capable period, active window = span of hit-capable periods inclusive, recovery = remainder, no-hit-period sequences treated as pure windup).
- `UCombatModeSubsystem::GetOrComputeTimingProfile` correctly caches per class and returns a safe default for a null class, but is not called from anywhere.
- `UWolfCombatSettings::DefaultInterruptResponses` is visible and editable in Project Settings, but not read anywhere.
- No existing behavior changes — this stage should be a no-op from the game's perspective, verifiable by confirming no existing call site was touched.
