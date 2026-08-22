// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCombatAbility.h"
#include "SwitchMode.generated.h"

/**
 * Ability that switches combat mode and applies Presage-related gameplay effects.
 */
UCLASS()
class WOLFCORE_API USwitchMode : public UBaseCombatAbility
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	/** Default constructor for USwitchMode. */
	USwitchMode();

	/** Activates the mode-switching ability, applying Presage-related gameplay effects and transitioning combat modes. Overrides UGameplayAbility::Activate(). */
	/**
	 * @param Handle The FGameplayAbilitySpecHandle identifying this ability instance.
	 * @param ActorInfo Pointer to the FGameplayAbilityActorInfo containing actor context.
	 * @param ActivationInfo Pointer to the FGameplayAbilityActivationInfo describing activation state.
	 * @param TriggerEventData Optional pointer to FGameplayEventData providing trigger event context.
	 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
};