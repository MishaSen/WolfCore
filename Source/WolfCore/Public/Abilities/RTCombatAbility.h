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
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	// ============================================================================================================================
	// Trace Configuration (Protected)
	// ============================================================================================================================

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Combat | Trace")
	float AttackRange = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat | Trace")
	float AttackRadius = 50.f;

	virtual void HandleAttackHitEvent(const FCombatPeriod& ContextPeriod) override;
};