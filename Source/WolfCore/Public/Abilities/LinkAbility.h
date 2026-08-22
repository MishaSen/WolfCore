// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RTCombatAbility.h"
#include "LinkAbility.generated.h"

class UGameplayEffect;

/**
 * Piercing skillshot ability that links every eligible IWolfCombatant it hits: applies
 * LinkEffectClass (GE_Link) per target with independent per-link duration (each application is
 * its own GameplayEffect instance, so per-link duration falls out of GAS for free), gated by a
 * Flow cost per link. This is groundwork for the ally-linking mechanic — no ally characters exist
 * yet, so this ability is tested against enemies as stand-in targets; see the TODO(teams) comment
 * in ProcessAttackHit(), and do not gate eligibility on any concrete enemy class as a stopgap.
 *
 * Subclasses URTCombatAbility to reuse its trace/hit plumbing (PerformAttackTrace,
 * HandleAttackHitEvent) as-is — SphereTraceMulti already returns every hit along the sweep, so
 * piercing comes free. Only ProcessAttackHit is overridden; what a hit *means* changes, not how
 * hits are found. Do not override HandleAttackHitEvent or PerformAttackTrace.
 *
 * Known follow-up, explicitly out of scope for this class: vision's aim-mode with time-slow
 * ("reaching into the flow state early") needs an aiming camera/input mode that doesn't exist yet.
 * This class uses inherited actor-forward aiming from URTCombatAbility — do not mistake that for
 * the intended final feel.
 *
 * Also out of scope: linked allies have no four-ability TB kit, no player-selection UI, and no
 * injection path of their own (TryInjectPresageAbility is player-avatar-only). This class only
 * makes Status.Link flow through the existing ApplyModeToActor gate; the command surface for
 * linked allies is a future workstream blocked on ally characters existing.
 */
UCLASS()
class WOLFCORE_API ULinkAbility : public URTCombatAbility
{
	GENERATED_BODY()

protected:
	/** GameplayEffect granting Status.Link to a hit target. Must be a Duration-policy GE (author
	  * in-editor as GE_Link, via UTargetTagsGameplayEffectComponent). This ability overrides the
	  * actual duration directly via SetDuration(LinkDurationSeconds, false) on the outgoing spec,
	  * so GE_Link's own authored duration value is a placeholder only — it is never read. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Link")
	TSubclassOf<UGameplayEffect> LinkEffectClass;

	/** Instant GameplayEffect that deducts Flow from the caster on each successful link, applied
	  * through the same SetByCaller/Data_Amount pattern UBaseCombatAbility::ApplyHitEffects uses,
	  * with a negative magnitude (-FlowCostPerLink). This is a standalone GE, not
	  * ResourceLoop's UResourceGainEffect — that class does not exist in this codebase yet. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Link")
	TSubclassOf<UGameplayEffect> FlowCostEffectClass;

	/** Duration, in seconds, applied to each link via FGameplayEffectSpec::SetDuration(). Drives
	  * GE_Link's actual duration regardless of how GE_Link's own DurationMagnitude is authored. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Link")
	float LinkDurationSeconds = 10.f;

	/** Flow cost deducted per actor linked. Checked against the caster's live FlowGauge
	  * immediately before each link; a hit that would exceed the caster's remaining Flow is not
	  * linked (and no cost is deducted for it). Because FlowGauge is re-read fresh on every call
	  * and hits are processed in trace order (nearest first), this naturally caps link count by
	  * available Flow without any extra bookkeeping — no explicit "stop" flag is needed. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Link")
	float FlowCostPerLink = 1.f;

	/** Full replacement of what a hit means for this ability — does not call
	  * Super::ProcessAttackHit(). See LinkAbility_Implementation.md for exact per-hit behavior. */
	virtual bool ProcessAttackHit(const FHitResult& Hit, const FCombatPeriod& ContextPeriod, const AActor* Avatar) override;
};