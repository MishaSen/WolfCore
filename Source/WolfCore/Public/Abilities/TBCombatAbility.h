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
	/** Activates the turn-based combat ability with cached hit time resolution. Overrides UGameplayAbility::Activate(). */
	/**
	 * @param Handle The FGameplayAbilitySpecHandle identifying this ability instance.
	 * @param ActorInfo Pointer to the FGameplayAbilityActorInfo containing actor context.
	 * @param ActivationInfo Pointer to the FGameplayAbilityActivationInfo describing activation state.
	 * @param TriggerEventData Optional pointer to FGameplayEventData providing trigger event context.
	 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	/** Initializes properties after the ability configuration is loaded. Overrides UGameplayAbility::PostInitProperties(). */
	virtual void PostInitProperties() override;

	// ============================================================================================================================
	// Presage API
	// ============================================================================================================================

	/** Returns the cached hit time value used for turn-based attack prediction and resolution timing. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Get Cached Hit Time"), Category = "WolfCore|Presage")
	/**
	 * @return Float representing the cached hit time in seconds (-1.0 if not yet calculated).
	 */
	float GetCachedHitTime() const { return CachedHitTime; }

	// ============================================================================================================================
	// Execution State (Protected)
	// ============================================================================================================================

protected:
	/** Cached hit time value in seconds used for turn-based attack prediction and Presage integration. */
	float CachedHitTime = -1.f;
};