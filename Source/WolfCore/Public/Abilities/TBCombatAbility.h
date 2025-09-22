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
struct FPeriod
{
	GENERATED_BODY()
	FPeriod()
		: PeriodType(EPeriod::MoveTo)
		, Duration(0.0f)
		, bIsInvulnerable(false)
		, Montage(nullptr)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPeriod PeriodType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsInvulnerable;

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

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnGameplayEventReceived(FGameplayEventData Payload);

private:
	int32 CurrentPeriodIndex = 0;

	void PlayCurrentPeriod(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                       const FGameplayAbilityActivationInfo& ActivationInfo);

	// Cached activation context for OnDelayFinished()
	FGameplayAbilitySpecHandle CachedHandle;
	const FGameplayAbilityActorInfo* CachedActorInfo;
	FGameplayAbilityActivationInfo CachedActivationInfo;
};
