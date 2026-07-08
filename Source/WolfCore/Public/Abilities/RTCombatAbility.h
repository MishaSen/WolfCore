// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCombatAbility.h"
#include "Core/WolfGameplayTags.h"
#include "RTCombatAbility.generated.h"

// ============================================================================================================================
// Forward Declarations
// ============================================================================================================================

class UAbilityFrameData;

/**
 * Real-time combat ability with trace-based hit detection.
 */
UCLASS()
class WOLFCORE_API URTCombatAbility : public UBaseCombatAbility
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Ability Hooks
	// ============================================================================================================================

public:
	/** Activates the combat ability with extended trace-based hit detection parameters. Overrides UGameplayAbility::Activate(). */
	/**
	 * @param Handle The FGameplayAbilitySpecHandle identifying this ability instance.
	 * @param ActorInfo Pointer to the FGameplayAbilityActorInfo containing actor context.
	 * @param ActivationInfo Pointer to the FGameplayAbilityActivationInfo describing activation state.
	 * @param TriggerEventData Optional pointer to FGameplayEventData providing trigger event context.
	 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	// ============================================================================================================================
	// Trace Configuration (Protected)
	// ============================================================================================================================

protected:
	/** Maximum attack range in centimeters for trace-based hit detection queries. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Combat | Trace")
	float AttackRange = 150.f;

	/** Radius in centimeters defining the area-of-effect for trace-based hit detection. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Combat | Trace")
	float AttackRadius = 50.f;

	/** Processes the attack hit event with trace-based collision detection and damage resolution. Overrides UBaseCombatAbility::HandleAttackHitEvent(). */
	/**
	 * @param ContextPeriod Reference to the FCombatPeriod currently executing its attack phase.
	 */
	virtual void HandleAttackHitEvent(const FCombatPeriod& ContextPeriod) override;

protected:
	/** Performs a sphere trace query from Start to End, storing results in OutHits. */
	void PerformAttackTrace(AActor* Avatar, const FVector& Start, const FVector& End, TArray<FHitResult>& OutHits);

	/** Processes a single hit result: validates target, resolves ASC, applies combat effects. Returns true if a valid target was processed. */
	bool ProcessAttackHit(const FHitResult& Hit, const FCombatPeriod& ContextPeriod, const AActor* Avatar, UAbilitySystemComponent* MyASC, float AbilityLevel);

	/** Applies a gameplay effect from SourceASC to TargetASC (or self if bToTarget is false) with the given parameters. */
	static void ApplyCombatEffect(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, const FGameplayEffectContextHandle& EffectContext, float AbilityLevel, const TSubclassOf<UGameplayEffect>& EffectClass, const FScalableFloat& Amount, bool bToTarget, FGameplayTag DataAmountTag = FWolfGameplayTags::Get().Data_Amount);

	/** Draws a debug cylinder along the trace path — green if any valid target was hit, red otherwise. */
	static void DrawAttackDebugCylinder(const AActor* Avatar, const FVector& StartPos, const FVector& EndPos, float Radius, bool bValidTargetFound);
 };
