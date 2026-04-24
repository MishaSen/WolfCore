// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCombatAbility.h"
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
};