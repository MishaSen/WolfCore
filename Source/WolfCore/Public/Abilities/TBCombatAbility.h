// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCombatAbility.h"
#include "TBCombatAbility.generated.h"

UENUM()
enum class EPeriod : uint8
{
	MoveTo,
	Evasion,
	Attack,
	Recovery
};

USTRUCT(BlueprintType)
struct FAttackPoint
{
	GENERATED_BODY()
	FAttackPoint()
		: Time(0.0f)
		, Damage(0.0f)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Time;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Damage;
};

USTRUCT(BlueprintType)
struct FPeriod
{
	GENERATED_BODY()
	FPeriod()
		: PeriodType(EPeriod::MoveTo)
		, Duration(0.0f)
		, bIsInvulnerable(false)
		, AttackPoints()
		, Montage(nullptr)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPeriod PeriodType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsInvulnerable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAttackPoint> AttackPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAnimMontage> Montage;
};

/**
 * 
 */
UCLASS()
class WOLFCORE_API UTBCombatAbility : public UBaseCombatAbility
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Periods")
	TArray<FPeriod> AbilitySequence;

	UFUNCTION()
	void OnDelayFinished();
	
	void PlayCurrentPeriod(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                       const FGameplayAbilityActivationInfo& ActivationInfo);
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

private:
	int32 CurrentPeriodIndex = 0;

	// Cached activation context for OnDelayFinished()
	FGameplayAbilitySpecHandle CachedHandle;
	const FGameplayAbilityActorInfo* CachedActorInfo;
	FGameplayAbilityActivationInfo CachedActivationInfo;
};
