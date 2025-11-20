// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatModeSubsystem.generated.h"

class ICombatModeListener;
/**
 * 
 */
UCLASS()
class WOLFCORE_API UCombatModeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void SetMode(FGameplayTag NewMode);
	void SwitchCombatMode();
	
	FGameplayTag GetCombatMode() const { return CurrentMode; }

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	FGameplayTag CurrentMode;

	FWolfGameplayTags WolfTag;

	UPROPERTY()
	UAbilitySystemComponent* PlayerASC;
	
	TArray<TScriptInterface<ICombatModeListener>> RegisteredListeners;

	UAbilitySystemComponent* GetPlayerASC() const;
};
