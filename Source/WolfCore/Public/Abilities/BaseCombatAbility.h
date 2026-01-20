// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BaseCombatAbility.generated.h"

UENUM()
enum class EPeriodType : uint8
{
	Windup,
	Attack, // Trigger Hit logic
	Recovery,
	Evasion,
	MoveTo // Only used for TB
};

USTRUCT(BlueprintType)
struct FCombatPeriod
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EPeriodType Type = EPeriodType::Attack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Duration = 0.5f; // Fallback if no Montage

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float HitDelay = 0.2f; // For Presage prediction if no Montage/Notify
};
/**
 * 
 */
UCLASS()
class WOLFCORE_API UBaseCombatAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	// --- Data ---
	UPROPERTY(EditDefaultsOnly, Category = "Combat Data")
	TArray<FCombatPeriod> AbilitySequence;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Data")
	FGameplayTag StartupInputTag;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Data")
	FGameplayTag HitEventTag;

	// --- Presage API ---
	UFUNCTION(BlueprintCallable, Category = "Presage")
	float CalculateProjectedImpactTime() const;

	bool IsInvulnerableAt(float RelativeTime) const;
	
protected:
	// --- Execution State ---
	int32 CurrentPeriodIndex = 0;

	// --- Main Loop ---
	UFUNCTION()
	void StartCombatSequence();

	UFUNCTION()
	void PlayNextPeriod();

	UFUNCTION()
	void OnPeriodCompleted();

	UFUNCTION()
	void OnEventReceived(FGameplayEventData EventData);

	virtual void HandleAttackHitEvent();
	
	float GetPeriodDuration(const FCombatPeriod& Period) const;
};