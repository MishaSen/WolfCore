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

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

protected:
	void ScheduleNextPhase(FTimerHandle& Handle, void (URTCombatAbility::*NextFunc)(), float PhaseDuration);
	
	void StartupPhase();
	void ActivePhase();
	void RecoveryPhase();
	
	void ClearTimers();
	void EndPhase();
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                           const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

private:
	UPROPERTY()
	UWorld* CachedWorld;
	
	FTimerHandle StartupTimerHandle;
	FTimerHandle ActiveTimerHandle;
	FTimerHandle RecoveryTimerHandle;
};