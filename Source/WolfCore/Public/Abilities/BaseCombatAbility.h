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
	Recovery,
	Evasion
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
	float Duration = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float HitDelay = 0.2f;
};
/**
 * 
 */
UCLASS()
class WOLFCORE_API UBaseCombatAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Combat Data")
	TArray<FCombatPeriod> AbilitySequence;

	UFUNCTION(BlueprintCallable, Category = "Presage")
	float CalculateProjectedImpactTime() const;

	bool IsInvulnerableAt(float RelativeTime) const;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag StartupInputTag;
	
protected:
	float GetPeriodDuration(const FCombatPeriod& Period) const;
};
