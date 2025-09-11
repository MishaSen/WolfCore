// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "WolfAbilitySystemComponent.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API UWolfAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	void AbilityInputTagPressed(const FGameplayTag& Tag);
	void AbilityInputTagReleased(const FGameplayTag& Tag);
	void AbilityInputTagHeld(const FGameplayTag& Tag);

protected:
	// --- UE Prediction Key Stuff ---
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetInputTagPressed(FGameplayTag Tag, FPredictionKey PredictionKey);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetInputTagReleased(FGameplayTag Tag, FPredictionKey PredictionKey);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetInputTagHeld(FGameplayTag Tag, FPredictionKey PredictionKey);
};