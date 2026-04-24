// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCombatAbility.h"
#include "TBCombatAbility.generated.h"

// ============================================================================================================================
// Forward Declarations
// ============================================================================================================================

/**
 * Turn-based combat ability with cached hit time tracking for Presage integration.
 */
UCLASS()
class WOLFCORE_API UTBCombatAbility : public UBaseCombatAbility
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Ability Hooks
	// ============================================================================================================================

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	virtual void PostInitProperties() override;

	// ============================================================================================================================
	// Presage API
	// ============================================================================================================================

	UFUNCTION(BlueprintCallable, Category = "Presage")
	float GetCachedHitTime() const { return CachedHitTime; }

	// ============================================================================================================================
	// Execution State (Protected)
	// ============================================================================================================================

protected:
	float CachedHitTime = -1.f;
};