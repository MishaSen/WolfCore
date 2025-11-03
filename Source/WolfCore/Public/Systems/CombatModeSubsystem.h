// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "CombatMode.h"
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
	void SetCombatMode(ECombatMode NewMode);
	ECombatMode GetCombatMode() const { return CurrentMode; }

private:
	ECombatMode CurrentMode = ECombatMode::OOC;
	TArray<TScriptInterface<ICombatModeListener>> RegisteredListeners;
	
	void ApplyModeToASC(ECombatMode Mode);
	
	UAbilitySystemComponent* GetPlayerASC() const;
	FGameplayTag GetTagForMode(ECombatMode Mode) const;
};
