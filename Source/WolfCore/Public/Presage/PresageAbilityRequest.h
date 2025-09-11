// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "Abilities/GameplayAbility.h"

#include "PresageAbilityRequest.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FPresageAbilityRequest
{
	GENERATED_BODY()

	UPROPERTY()
	UAbilitySystemComponent* OwnerASC = nullptr;

	UPROPERTY()
	FGameplayAbilitySpecHandle SpecHandle;

	UPROPERTY()
	float ScheduledTime = 0.f;

	UPROPERTY()
	FGameplayTag AbilityTag;
	
	FPresageAbilityRequest() {}

	FPresageAbilityRequest(
		UAbilitySystemComponent* InASC,
		const FGameplayAbilitySpecHandle InSpecHandle,
		const float InScheduledTime,
		const FGameplayTag& InTag)
		: OwnerASC(InASC), SpecHandle(InSpecHandle), ScheduledTime(InScheduledTime), AbilityTag(InTag)
	{}
};
