// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BaseCombatAbility.generated.h"

// ============================================================================================================================
// Enums
// ============================================================================================================================

UENUM()
enum class EPeriodType : uint8
{
	Windup,
	Attack,
	Wait,
	Evasion,
	MoveTo
};

/**
 * Describes a single period within a combat ability sequence (montage, duration, attribute effects).
 */
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float Range = 150.f;

	UPROPERTY(BlueprintReadOnly)
	FVector MoveToDestination = FVector::ZeroVector;
};

/**
 * Base combat ability with period-based sequence execution and Presage integration.
 */
UCLASS()
class WOLFCORE_API UBaseCombatAbility : public UGameplayAbility
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	UBaseCombatAbility();

	// ============================================================================================================================
	// Combat Data Configuration
	// ============================================================================================================================

	UPROPERTY(EditDefaultsOnly, Category = "Combat Data")
	TArray<FCombatPeriod> AbilitySequence;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Data")
	FGameplayTag StartupInputTag;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Data")
	FGameplayTag HitEventTag;

	// ============================================================================================================================
	// Presage API
	// ============================================================================================================================

	UFUNCTION(BlueprintCallable, Category = "Presage")
	float CalculateProjectedImpactTime() const;

	int32 GetCurrentPeriodIndex() const { return CurrentPeriodIndex; }
	void SetCurrentPeriodIndex(int32 NewIndex) { CurrentPeriodIndex = NewIndex; }

	bool IsInvulnerableAt(float RelativeTime) const;

	static float GetPeriodDuration(const FCombatPeriod& Period);

	UFUNCTION(BlueprintCallable, Category = "Presage")
	float GetPeriodProgress() const;

	UFUNCTION(BlueprintCallable, Category = "Presage")
	static float CalculateMovementDuration(float TotalDistance, float MaxVelocity, float Acceleration, float StartVelocity);

	// ============================================================================================================================
	// Execution State (Protected)
	// ============================================================================================================================

protected:
	int32 CurrentPeriodIndex = 0;
	float CurrentPeriodStartTime = 0.f;

	// --- Main Loop ---
	UFUNCTION()
	void StartCombatSequence();

	void ExecuteWait(const FCombatPeriod& Period);
	void ExecuteAnimatedPeriod(const FCombatPeriod& Period);
	UFUNCTION()
	void PlayNextPeriod();
	void ExecuteMoveTo(FCombatPeriod& Period);

	AActor* GetTargetFromBlackboard() const;
	UFUNCTION()
	void OnPeriodCompleted();

	UFUNCTION()
	void OnEventReceived(FGameplayEventData EventData);

	virtual void HandleAttackHitEvent(const FCombatPeriod& CurrentAttackPeriod);

	// --- Attribute Effects ---
	UPROPERTY(EditDefaultsOnly, Category = "Combat | Effects")
	TSubclassOf<UGameplayEffect> FlowGainEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Combat | Effects")
	TSubclassOf<UGameplayEffect> AdrenalineGainEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Combat | Effects")
	TSubclassOf<UGameplayEffect> DamageEffect;
};