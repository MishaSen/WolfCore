// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCombatAbility.h"
#include "TBCombatAbility.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API UTBCombatAbility : public UBaseCombatAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
	
	virtual void PostInitProperties() override;
	
	float GetCachedHitTime() const { return CachedHitTime; } // Helper for Presage

private:
	float CachedHitTime = -1.f;
};