// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/WolfGameplayTags.h"
#include "Presage/ActorSnapshot.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/IWolfCombatant.h"
#include "CombatModeSubsystem.generated.h"

struct FStreamableHandle;
class UAbilitySystemComponent;
class UWolfCombatant;

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

	/** Initializes the subsystem when it is registered with the world, setting up internal state and tag references. */
	/**
	 * @param Collection Reference to the FSubsystemCollectionBase containing all initialized subsystems.
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Called when the world begins play, initializing combat mode state and loading default configuration. */
	/**
	 * @param InWorld Pointer to the UWorld being loaded for subsystem initialization.
	 */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ============================================================================================================================
	// Public API - Mode Management
	// ============================================================================================================================

public:
	/** Sets the global combat mode to a new value, triggering mode transitions and updating all registered combatants. */
	/**
	 * @param NewMode The FGameplayTag representing the new combat mode (e.g., RT, TB).
	 */
	void SetMode(FGameplayTag NewMode);

	/** Switches between combat modes using the player's current selection from the game instance configuration. */
	void SwitchCombatMode();

	/** Applies a specific combat mode to an individual actor, updating their ability system and state accordingly. */
	/**
	 * @param Combatant Pointer to the AActor whose combat mode is being updated (can be any actor type).
	 * @param NewMode The FGameplayTag representing the new combat mode to apply.
	 */
	void ApplyModeToActor(const TScriptInterface<IWolfCombatant>& Combatant, FGameplayTag NewMode);

	/** Returns the current global combat mode as a GameplayTag for Blueprint queries and runtime checks. */
	UFUNCTION(BlueprintPure, Meta = (DisplayName = "Get Current Mode"), Category = "WolfCore|Combat")
	FGameplayTag GetCurrentMode() const { return CurrentMode; }

	/** Returns the current timeline position in seconds representing elapsed time within the active combat mode. */
	UFUNCTION(BlueprintPure, Meta = (DisplayName = "Get Timeline Time"), Category = "WolfCore|Combat")
	float GetCurrentTimelineTime() const { return CurrentTimelineTime; }

	/** Boolean flag indicating whether the current combat mode is set to turn-based (TB) rather than real-time (RT). */
	bool bIsInTB;

	// ============================================================================================================================
	// Public API - Snapshots & Timeline
	// ============================================================================================================================

	/** Captures the current world state into a FTemporalStates container at the specified timestamp for timeline scrubbing. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Capture World State"), Category = "WolfCore|Combat|Snapshots")
	/**
	 * @param Timestamp The time anchor in seconds at which to capture the world state snapshot.
	 * @return FTemporalStates containing the captured world state mapped to the specified timestamp.
	 */
	FTemporalStates CaptureCurrentWorldState(float Timestamp);

	/** Advances or rewinds the prediction timeline to a specific timestamp for scrubbing and visualization. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Scrub Timeline"), Category = "WolfCore|Combat")
	/**
	 * @param NewTime The target timeline position in seconds to scrub the prediction state to.
	 */
	void ScrubTimeline(float NewTime);

	/** Returns a reference to the master snapshot captured at the start of the current simulation session. */
	const FTemporalStates& GetMasterSnapshot() const { return MasterStartSnapshot; }

	// ============================================================================================================================
	// Public API - Combatant Tracking
	// ============================================================================================================================

	/** Registers a combatant as an active participant, enabling it to receive mode updates and prediction data. */
	/**
	 * @param Combatant TScriptInterface<IWolfCombatant> representing the combatant being registered.
	 */
	void RegisterCombatant(const TScriptInterface<IWolfCombatant>& Combatant);

	/** Unregisters a combatant from active participation, removing it from mode updates and prediction processing. */
	/**
	 * @param Combatant TScriptInterface<IWolfCombatant> representing the combatant being removed.
	 */
	void UnregisterCombatant(const TScriptInterface<IWolfCombatant>& Combatant);

	/** Returns the array of weak references to tracked combatants currently registered with this subsystem. */
	const TArray<TScriptInterface<IWolfCombatant>>& GetTrackedCombatants() const { return TrackedCombatants; }

	/** Returns the fixed step size (seconds) used to produce the current PredictionBuffer contents.
	  * This value is set once at bake-start and read by every UWolfPresageComponent::GetSnapshotAtTime()
	  * call, ensuring index math uses the actual step size rather than a compile-time constant. */
	float GetBakedStepSize() const { return BakedStepSize; }

	// ============================================================================================================================
	// Events
	// ============================================================================================================================

	/** Multicast delegate broadcast whenever the global combat mode changes, notifying all registered listeners. */
	UPROPERTY(BlueprintAssignable, Category = "WolfCore|Combat")
	FOnCombatModeChanged OnCombatModeChanged;

protected:
	/** Temporal states container holding the master snapshot captured at simulation start for state restoration. */
	FTemporalStates MasterStartSnapshot;

	/** Current timeline position in seconds representing elapsed time within the active combat mode session. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Timeline")
	float CurrentTimelineTime = 0.f;

	/** Maximum allowed duration in seconds for the prediction timeline before requiring regeneration or reset. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Timeline")
	float MaxTimelineDuration = 5.f;

	private:
	/** The fixed step size (seconds) used to produce the current PredictionBuffer contents across
	  * every combatant in the active bake. Set once at the start of ExecuteFutureBake() from
	  * UWolfCombatSettings::PresageSimulationStep; every UWolfPresageComponent::GetSnapshotAtTime()
	  * call reads this via GetBakedStepSize(), so a designer-tuned change to the simulation rate
	  * only needs to update this one path. */
	float BakedStepSize = 0.1f; // Fallback only; overwritten from UWolfCombatSettings in SetMode().

	/** Handles completion of presage effect loading, applying the loaded gameplay effect to the player's ability system. */
	void OnPresageEffectLoaded();

	/** Initializes subsystem defaults by setting up internal state, tag references, and configuration values at startup. */
	void InitializeSubsystemDefaults();

	/** Attempts to load the player-selected combat mode from game instance configuration for initial mode setup. */
	/**
	 * @return True if a valid player-selected mode was loaded; false otherwise (default level mode will be applied).
	 */
	bool TryLoadPlayerSelectedMode();

	/** Applies the default combat mode from level settings when no player selection is available or valid. */
	void ApplyDefaultLevelMode();

	/** Handles presage drain effect application to an actor's ability system based on their current combat mode state. */
	/**
	 * @param ASC Pointer to the UAbilitySystemComponent receiving the drain effect modification.
	 * @param CurrentActorMode The FGameplayTag representing the actor's current combat mode being evaluated.
	 */
	void HandlePresageDrainEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode);

	/** Retrieves the player controller's Ability System Component (ASC) for gameplay effect application and state management. */
	UAbilitySystemComponent* GetPlayerASC() const;

	/** Returns the time dilation multiplier configured for a specific combat mode to control temporal prediction scaling. */
	/**
	 * @param Mode The FGameplayTag representing the combat mode whose dilation value is being queried.
	 * @return Float representing the time dilation factor (e.g., 1.0 for RT, 0.5 for TB).
	 */
	static float GetDilationForMode(const FGameplayTag& Mode);

	/** GameplayTag representing the current global combat mode (Real-Time or Turn-Based) active in this session. */
	FGameplayTag CurrentMode;

	/** Singleton gameplay tags structure providing framework-wide tag access for combat mode identification and filtering. */
	FWolfGameplayTags WolfTag;

	/** Shared pointer to a streamable handle managing asynchronous loading of the presage effect class asset. */
	TSharedPtr<FStreamableHandle> PresageClassLoadHandle;

	/** Subclass of GameplayEffect applied during presage simulation for drain effects and temporal state management. */
	UPROPERTY()
	TSubclassOf<UGameplayEffect> PresageEffectClass;

	/** Array of weak script interfaces to combatants currently tracked by this subsystem for mode updates and prediction processing. */
	UPROPERTY()
	TArray<TScriptInterface<IWolfCombatant>> TrackedCombatants;
};