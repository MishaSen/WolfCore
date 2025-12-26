// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCombatAbility.h"
#include "TBCombatAbility.generated.h"

UENUM(meta = (ScriptName = "CombatPeriodEnum"))
enum class EPeriod : uint8
{
	MoveTo,
	Evasion,
	Attack,
	Recovery
};

USTRUCT(BlueprintType, meta = (ScriptName = "CombatPeriodStruct"))
struct FPeriod
{
	GENERATED_BODY()
	FPeriod()
		: PeriodType(EPeriod::MoveTo)
		, Duration(0.0f)
		, HitDelay(0.0f)
		, bUseMontageLength(false)
		, Montage(nullptr)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPeriod PeriodType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="!bUseMontageLength"))
	float Duration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="PeriodType == EPeriod::Attack"))
	float HitDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseMontageLength;

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

	float GetProjectedAttackTime() const { return CachedProjectedHitTime; }

	void RefreshCachedData();

	virtual void PostInitProperties() override;

	bool IsInvulnerableAt(float RelativeTime) const;

	UFUNCTION()
	void OnPeriodFinished();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Presage | Setup")
	FGameplayTag ImpactTrackingTag;

private:
	int32 CurrentPeriodIndex = 0;

	float CachedProjectedHitTime = -1.f;

	float CalculateProjectedAttackTime() const;

	void PlayCurrentPeriod(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                       const FGameplayAbilityActivationInfo& ActivationInfo);

	static float GetTruePeriodDuration(const FPeriod& Period);

	virtual void HandleGameplayEventHit_Implementation(FGameplayEventData Payload) override;

	// Cached activation context for OnDelayFinished()
	FGameplayAbilitySpecHandle CachedHandle;
	const FGameplayAbilityActorInfo* CachedActorInfo;
	FGameplayAbilityActivationInfo CachedActivationInfo;
};