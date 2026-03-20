// Fill out your copyright notice in the Description page of Project Settings.d

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Presage/ActorSnapshot.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatModeSubsystem.generated.h"

class AWolfCharacterBase;
struct FStreamableHandle;
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

	UFUNCTION(BlueprintCallable, Category = "Wolf|Combat|Snapshots")
	FTemporalStates CaptureCurrentWorldState(float Timestamp);

	UFUNCTION(BlueprintCallable, Category = "Wolf|Combat")
	void ScrubTimeline(float NewTime);
	const FTemporalStates& GetMasterSnapshot() const { return MasterStartSnapshot; }

	void RegisterCombatant(AWolfCharacterBase* Character);
	void UnregisterCombatant(AWolfCharacterBase* Character);
	const TArray<TWeakObjectPtr<AWolfCharacterBase>>& GetTrackedCombatants() const { return TrackedCombatants; }

	UFUNCTION(BlueprintCallable, Category = "Wolf|Presage")
	void GenerateFutureState(float Duration);

public:
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnCombatModeChanged OnCombatModeChanged;

protected:
	UPROPERTY()
	FTemporalStates MasterStartSnapshot;

	UPROPERTY(BlueprintReadOnly, Category = "Wolf|Timeline")
	float CurrentTimelineTime = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Wolf|Timeline")
	float MaxTimelineDuration = 5.f;

private:
	void OnPresageEffectLoaded();
	void InitializeSubsystemDefaults();
	bool TryLoadPlayerSelectedMode();
	void ApplyDefaultLevelMode();
	void HandlePresageDrainEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode);

	UAbilitySystemComponent* GetPlayerASC() const;
	static float GetDilationForMode(const FGameplayTag& Mode);

private:
	FGameplayTag CurrentMode;
	FWolfGameplayTags WolfTag;
	TSharedPtr<FStreamableHandle> PresageClassLoadHandle;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> PresageEffectClass;
	
	UPROPERTY()
	TArray<TWeakObjectPtr<AWolfCharacterBase>> TrackedCombatants;
};