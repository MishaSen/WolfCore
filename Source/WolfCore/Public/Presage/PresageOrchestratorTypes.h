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

	/** Time from ability start to the first actual impact: the first hit-capable period's start
	  * offset plus that period's impact offset (montage notify trigger time, or HitDelay if no
	  * montage/no matching notify). This is the single source of truth for "when does the hit
	  * actually land" — distinct from ActiveWindowStart, which is only the start of the hit-capable
	  * period and does not account for HitDelay or notify placement within it. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float FirstImpactTime = 0.f;
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

	/** True if this intent was interrupted during temporal resolution and InterruptedAtTime holds
	  * a meaningful value. Set by FPresageOrchestrator::ResolveInterrupts. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	bool bHasInterruptedAtTime = false;

	/** Only meaningful if bHasInterruptedAtTime is true. Timeline time at which the interrupting
	  * attack actually lands — Execution cuts this entry's ability short at this time instead of
	  * letting it run to its natural completion. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float InterruptedAtTime = 0.f;
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