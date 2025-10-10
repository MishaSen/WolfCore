// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Presage/PresageAbilityRequest.h"
#include "WolfAbilitySystemComponent.generated.h"

UENUM()
enum ECombatMode
{
	RT, TB
};
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
	void AddCharacterAbilities(TArray<TSubclassOf<UGameplayAbility>> StartupAbilities);
	void SetModeStateTags(ECombatMode NewMode);
	FPresageAbilityRequest BuildInitialPresageRequest(const FGameplayTag& Tag, const TArray<TWeakObjectPtr<AActor>>& Targets);
};