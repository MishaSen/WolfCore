// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BaseCombatAbility.generated.h"

UENUM()
enum class EPeriodType : uint8
{
	Windup,
	Attack,
	Wait,
	Rotate,
	Evasion,
	MoveTo
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

	// --- Attribute Data ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute Effects")
	FScalableFloat FlowGain = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute Effects")
	FScalableFloat AdrenalineGain = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute Effects")
	FScalableFloat Damage = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FVector MoveToDestination = FVector::ZeroVector;
};
/**
 * 
 */
UCLASS()
class WOLFCORE_API UBaseCombatAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBaseCombatAbility();
	
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

	int32 GetCurrentPeriodIndex() const { return CurrentPeriodIndex; }
	void SetCurrentPeriodIndex(int32 NewIndex) { CurrentPeriodIndex = NewIndex; }

	bool IsInvulnerableAt(float RelativeTime) const;
	
protected:
	// --- Execution State ---
	int32 CurrentPeriodIndex = 0;

	// --- Main Loop ---
	UFUNCTION()
	void StartCombatSequence();

	void ExecuteRotate(const FCombatPeriod& element);
	void ExecuteWait(const FCombatPeriod& element);
	void ExecuteAnimatedPeriod(const FCombatPeriod& element);
	UFUNCTION()
	void PlayNextPeriod();
	void ExecuteMoveTo(FCombatPeriod Period);

	AActor* GetTargetFromBlackboard() const;
	UFUNCTION()
	void OnPeriodCompleted();

	UFUNCTION()
	void OnEventReceived(FGameplayEventData EventData);

	virtual void HandleAttackHitEvent();

	static float GetPeriodDuration(const FCombatPeriod& Period);

	// --- Attribute Effects ---
	
	UPROPERTY(EditDefaultsOnly, Category = "Combat | Effects")
	TSubclassOf<UGameplayEffect> FlowGainEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Combat | Effects")
	TSubclassOf<UGameplayEffect> AdrenalineGainEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Combat | Effects")
	TSubclassOf<UGameplayEffect> DamageEffect;

private:
	static float CalculateMovementDuration(float TotalDistance, float MaxVelocity, float Acceleration, float StartVelocity);
};