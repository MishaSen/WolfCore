// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCombatAbility.h"
#include "RTCombatAbility.generated.h"

class UAbilityFrameData;

/**
 * 
 */
UCLASS()
class WOLFCORE_API URTCombatAbility : public UBaseCombatAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Combat | Effects")
	TSubclassOf<UGameplayEffect> FlowGainEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Combat | Effects")
	TSubclassOf<UGameplayEffect> AdrenalineGainEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Combat | Effects")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Combat | Trace")
	float AttackRange = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat | Trace")
	float AttackRadius = 50.f;

	virtual void HandleAttackHitEvent() override;
};