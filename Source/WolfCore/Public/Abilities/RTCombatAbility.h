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
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> AttackMontage;
	
	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	TObjectPtr<UAbilityFrameData> FrameData;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
	void OnNotifyReceived(FName NotifyName);

protected:
	UFUNCTION()
	void StartupPhase();

	UFUNCTION()
	void ActivePhase();

	UFUNCTION()
	void RecoveryPhase();

	UFUNCTION()
	void EndPhase();
	
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                           const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

	UPROPERTY(EditDefaultsOnly, Category = "GAS | Effects")
	TSubclassOf<UGameplayEffect> HitAdrenalineGE;

private:
	UPROPERTY()
	UWorld* CachedWorld;
};