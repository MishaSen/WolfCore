// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "PresageImpactLedger.generated.h"

class AActor;
class UGameplayEffect;

/**
 * A single predicted attribute change, derived from one FCombatHitEffect on the resolving period.
 * Not applied through real GameplayEffect machinery during the bake — see
 * UWolfPresageComponent::ResolveSimulatedImpact and PresagePreviewStage1's contract (out-of-contract:
 * GameplayEffect object instances during preview).
 */
USTRUCT(BlueprintType)
struct WOLFCORE_API FPredictedEffectDelta
{
	GENERATED_BODY()

	/** The attribute this delta was applied to. Only meaningful if bIsPredictable is true. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	FGameplayAttribute Attribute;

	/** Signed amount applied: FCombatHitEffect::Amount.GetValueAtLevel(...) * SignMultiplier. Only
	  * meaningful if bIsPredictable is true. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float Amount = 0.f;

	/** True = this delta was applied to the attacker (self-target); false = the victim. Mirrors
	  * the source FCombatHitEffect::bSelfTarget. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	bool bSelfTarget = false;

	/** The GameplayEffect class this delta was derived from. PresagePreviewStage3 re-applies the
	  * real GE from this, through the same construction UBaseCombatAbility::ApplyHitEffects uses. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TSubclassOf<UGameplayEffect> EffectClass;

	/** False if the source FCombatHitEffect::TargetAttributeTag was unset — prediction-opaque per
	  * stage 1's magnitude-predictability rule. When false, Attribute/Amount are meaningless and
	  * nothing was applied numerically for this delta; a contract warning was logged instead. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	bool bIsPredictable = false;
};

/**
 * One resolved impact from the bake: an attack period crossing its impact offset against a
 * resolved victim. Owned by UCombatModeSubsystem::BakeImpactLedger, flat and time-ordered (append
 * order during the bake is already time-ordered — the bake advances all combatants in lockstep
 * per step). Consumed by UCombatModeSubsystem::CheckExecutionDamageExit (stage 1's hard-exit hook)
 * and, from PresagePreviewStage3 onward, by real ledger-impact application during execution
 * playback.
 */
USTRUCT(BlueprintType)
struct WOLFCORE_API FPresageImpactEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TWeakObjectPtr<AActor> Attacker;

	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TWeakObjectPtr<AActor> Victim;

	/** Bake-relative time, in seconds, at which this impact occurs. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float ImpactTime = 0.f;

	/** False if evaded or out of range — recorded anyway, for UI later. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	bool bConnected = false;

	/** Computed at ledger-append time (link state at bake time is the state that was baked): true
	  * if Victim is player-controlled OR Victim's ASC carries Status.Link. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	bool bVictimIsPlayerOrLinked = false;

	/** One entry per FCombatHitEffect in the resolving period's HitEffects (NOT
	  * ConditionalHitEffects — TB prediction does not evaluate conditional effects; see
	  * AssistCanary.md's own note on this same gap). */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TArray<FPredictedEffectDelta> Deltas;

	/** ResourceLoop stage 1: resource gains applied to the PLAYER for this hit, computed at bake
	  * time with the pure FWolfResourceRules::ComputeResourceGain (passing InputState_TB
	  * explicitly). Zero for entries that are not connected hits. Execution playback applies the
	  * RECORDED values (UCombatModeSubsystem::ApplyDueLedgerImpacts) so real gains equal the
	  * preview by construction — the same exactness rule as the predicted damage deltas. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float FlowGain = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float AdrenalineGain = 0.f;
};