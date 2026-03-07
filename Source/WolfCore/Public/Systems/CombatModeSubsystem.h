// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatModeSubsystem.generated.h"

class AWolfCharacterBase;
struct FStreamableHandle;
class UPresageSubsystem;
/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatModeChanged, FGameplayTag, NewMode);

UCLASS()
class WOLFCORE_API UCombatModeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	
	void SetMode(FGameplayTag NewMode);
	void SwitchCombatMode();
	void ApplyModeToActor(AActor* Combatant, FGameplayTag NewMode);
	
	UFUNCTION(BlueprintPure, Category = "Combat")
	FGameplayTag GetCurrentMode() const { return CurrentMode; }

	void RegisterCombatant(AWolfCharacterBase* Character);
	void UnregisterCombatant(AWolfCharacterBase* Character);
	const TArray<TWeakObjectPtr<AWolfCharacterBase>>& GetTrackedCombatants() const { return TrackedCombatants; }

public:
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnCombatModeChanged OnCombatModeChanged;

private:
	void OnPresageEffectLoaded();
	void InitializeSubsystemDefaults();
	bool TryLoadPlayerSelectedMode();
	void ApplyDefaultLevelMode();
	void HandlePresageDrainEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode);

	UAbilitySystemComponent* GetPlayerASC() const;
	static float GetDilationForMode(const FGameplayTag& Mode);

	UPROPERTY()
	TArray<TWeakObjectPtr<AWolfCharacterBase>> TrackedCombatants;
	
private:
	FGameplayTag CurrentMode;
	FWolfGameplayTags WolfTag;
	TSharedPtr<FStreamableHandle> PresageClassLoadHandle;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> PresageEffectClass;

	UPROPERTY()
	TObjectPtr<UPresageSubsystem> CachedPresage;
};