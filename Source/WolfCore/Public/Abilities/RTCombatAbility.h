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
	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	TObjectPtr<UAbilityFrameData> FrameData;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	void EnterActivePhase();
	void EnterRecoveryPhase();
	void EndAbilityPhase();

protected:
	virtual void OnStartupPhase();
	virtual void OnActivePhase();
	virtual void OnRecoveryPhase();

private:
	FTimerHandle StartupTimerHandle;
	FTimerHandle ActiveTimerHandle;
	FTimerHandle RecoveryTimerHandle;
};
