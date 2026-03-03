// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "Interfaces/CombatModeListener.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatModeSubsystem.generated.h"

class UPresageSubsystem;
/**
 * 
 */
UCLASS()
class WOLFCORE_API UCombatModeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	
	void SetMode(FGameplayTag NewMode);
	void SwitchCombatMode();
	
	UFUNCTION(BlueprintPure, Category = "Combat")
	FGameplayTag GetCurrentMode() const { return CurrentMode; }
	
	void RegisterCombatListener(AActor* Combatant);
	void UnregisterCombatListener(const AActor* Combatant);

private:
	void InitializeSubsystemDefaults();
	bool TryLoadPlayerSelectedMode();
	void ApplyDefaultLevelMode();
	void UpdateCombatantModeTags(FGameplayTag NewMode);
	void ApplyModeToActor(AActor* Combatant, FGameplayTag NewMode);

	UAbilitySystemComponent* GetPlayerASC() const;
	float GetDilationForMode(const FGameplayTag& Mode) const;
	
private:
	FGameplayTag CurrentMode;
	FWolfGameplayTags WolfTag;

	UPROPERTY()
	TObjectPtr<UPresageSubsystem> CachedPresage;

	UPROPERTY()
	TSet<AActor*> Combatants;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Settings")
	TMap<FGameplayTag, float> ModeTimeDilationMap; // Possibly implement as a data asset in the future
};