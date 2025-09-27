// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BaseCombatAbility.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API UBaseCombatAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag StartupInputTag;
	
protected:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat")
	void HandleGameplayEventHit(FGameplayEventData Payload);
	virtual void HandleGameplayEventHit_Implementation(FGameplayEventData Payload);
};
