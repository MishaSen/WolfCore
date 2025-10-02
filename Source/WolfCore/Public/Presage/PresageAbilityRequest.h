// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/TBCombatAbility.h"

#include "PresageAbilityRequest.generated.h"


/**
 * 
 */
USTRUCT(BlueprintType)
struct FPresageAbilityRequest
{
	GENERATED_BODY()

	FPresageAbilityRequest(){}

	FPresageAbilityRequest(
		const TObjectPtr<UAbilitySystemComponent>& InASC,
		FGameplayTag InInputTag,
		float InRequestedTime,
		const TArray<FPeriod>& InAbilitySequence,
		const TArray<TWeakObjectPtr<AActor>>& InTargets
	):
		OwnerASC(InASC),
		InputTag(InInputTag),
		ScheduledTime(InRequestedTime),
		AbilitySequence(InAbilitySequence),
		Targets(InTargets)
	{}

	TObjectPtr<UAbilitySystemComponent> GetOwnerASC() const { return OwnerASC; }
	const FGameplayTag& GetInputTag() const { return InputTag; }
	float GetRequestedTime() const { return ScheduledTime; }
	const TArray<FPeriod>& GetAbilitySequence() const { return AbilitySequence; }
	const TArray<TWeakObjectPtr<AActor>>& GetTargets() const { return Targets; }

private:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> OwnerASC = nullptr;

	UPROPERTY()
	FGameplayTag InputTag;

	UPROPERTY()
	float ScheduledTime = 0.f;

	UPROPERTY()
	TArray<FPeriod> AbilitySequence;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Targets;
};