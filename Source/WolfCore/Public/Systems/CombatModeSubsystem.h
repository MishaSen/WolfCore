// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/WolfGameplayTags.h"
#include "Presage/ActorSnapshot.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatModeSubsystem.generated.h"

struct FStreamableHandle;
class AWolfCharacterBase;
class UAbilitySystemComponent;

/**
 * Delegate broadcast when the global combat mode changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatModeChanged, FGameplayTag, NewMode);

/**
 * World subsystem managing combat mode state (Real-Time / Turn-Based).
 * Handles mode transitions, time dilation, temporal snapshots, and presage simulation coordination.
 */
UCLASS()
class WOLFCORE_API UCombatModeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ============================================================================================================================
	// Public API - Mode Management
	// ============================================================================================================================

public:
	void SetMode(FGameplayTag NewMode);
	void SwitchCombatMode();
	void ApplyModeToActor(AActor* Combatant, FGameplayTag NewMode);

	UFUNCTION(BlueprintPure, Category = "Combat")
	FGameplayTag GetCurrentMode() const { return CurrentMode; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetCurrentTimelineTime() const { return CurrentTimelineTime; }

	bool bIsInTB;

	// ============================================================================================================================
	// Public API - Snapshots & Timeline
	// ============================================================================================================================

	UFUNCTION(BlueprintCallable, Category = "Wolf|Combat|Snapshots")
	FTemporalStates CaptureCurrentWorldState(float Timestamp);

	UFUNCTION(BlueprintCallable, Category = "Wolf|Combat")
	void ScrubTimeline(float NewTime);

	const FTemporalStates& GetMasterSnapshot() const { return MasterStartSnapshot; }

	UFUNCTION(BlueprintCallable, Category = "Wolf|Presage")
	void GenerateFutureState(float Duration);

	// ============================================================================================================================
	// Public API - Combatant Tracking
	// ============================================================================================================================

	void RegisterCombatant(AWolfCharacterBase* Character);
	void UnregisterCombatant(AWolfCharacterBase* Character);
	const TArray<TWeakObjectPtr<AWolfCharacterBase>>& GetTrackedCombatants() const { return TrackedCombatants; }

	// ============================================================================================================================
	// Events
	// ============================================================================================================================

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnCombatModeChanged OnCombatModeChanged;

protected:
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

	FGameplayTag CurrentMode;
	FWolfGameplayTags WolfTag;
	TSharedPtr<FStreamableHandle> PresageClassLoadHandle;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> PresageEffectClass;

	UPROPERTY()
	TArray<TWeakObjectPtr<AWolfCharacterBase>> TrackedCombatants;
};