// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "Core/WolfGameplayTags.h"
#include "Presage/Snapshot.h"
#include "Subsystems/WorldSubsystem.h"
#include "PresageSubsystem.generated.h"

struct FCombatPeriod;
struct FActorSnapshot;
struct FPresageAbilityRequest;

class AWolfCharacterBase;
class UAbilitySystemComponent;
class UGameplayAbility;

/**
 * Single frame capturing a character's state at a specific timestamp during presage simulation.
 */
USTRUCT(BlueprintType)
struct FCharacterFrame
{
	GENERATED_BODY()

	/** Timestamp in seconds representing the position of this frame within the prediction timeline. */
	UPROPERTY()
	float Timestamp = 0.f;

	/** World-space location vector capturing the character's position at this simulation timestamp. */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** World-space rotation value capturing the character's orientation at this simulation timestamp. */
	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	/** Playback position in seconds within the current animation montage at capture time. */
	UPROPERTY()
	float MontagePosition = 0.f;

	/** Weak reference to the active animation montage playing during this frame's simulation period. */
	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> ActiveMontage = nullptr;

	/** Current health value in percentage representing the character's vitality at capture time. */
	UPROPERTY()
	float CurrentHealth = 0.f;
};

/**
 * Timeline track containing a sequence of character frames for visualization during presage prediction.
 */
USTRUCT(BlueprintType)
struct FCharacterTimelineTrack
{
	GENERATED_BODY()

	/** Array of FCharacterFrame entries representing the complete timeline of captured simulation data. */
	UPROPERTY()
	TArray<FCharacterFrame> Frames;
};

/**
 * Event on the presage timeline representing a predicted combat interaction between attacker and victim.
 */
USTRUCT(BlueprintType)
struct FPresageTimelineEvent
{
	GENERATED_BODY()

	/** Default constructor creating an empty timeline event with zeroed parameters. */
	FPresageTimelineEvent() = default;

	/** Full constructor initializing all fields for a predicted combat interaction event. */
	/**
	 * @param InAttacker Pointer to the AActor initiating the attack during prediction.
	 * @param InVictim Pointer to the AActor receiving the predicted attack.
	 * @param InTime The scheduled impact time in seconds for temporal prediction queries.
	 * @param InAbilityTag The GameplayTag representing the ability used in this interaction.
	 */
	FPresageTimelineEvent(AActor* InAttacker, AActor* InVictim, float InTime, FGameplayTag InAbilityTag)
		: Attacker(InAttacker)
		, Victim(InVictim)
		, Time(InTime)
		, ResultTag(InAbilityTag)
	{}

	/** Strong reference to the attacker actor initiating the predicted combat interaction. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TObjectPtr<AActor> Attacker;

	/** Strong reference to the victim actor receiving the predicted attack during simulation. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TObjectPtr<AActor> Victim;

	/** Impact time in seconds representing when this predicted interaction occurs on the timeline. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	float Time = 0.f;

	/** GameplayTag representing the result type of this predicted combat interaction (e.g., Hit, Dodge). */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	FGameplayTag ResultTag;
};

/**
 * Tickable world subsystem managing the presage prediction loop.
 * Coordinates ability queuing, simulation baking, timeline scrubbing, and participant management.
 */
UCLASS()
class WOLFCORE_API UPresageSubsystem : public UTickableWorldSubsystem, public ISnapshot
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Public API - Database of Predictions
	// ============================================================================================================================

public:
	/** Map of character actors to their visual timeline tracks for prediction visualization and debugging. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TMap<AWolfCharacterBase*, FCharacterTimelineTrack> VisualTracks;

	/** Executes the simulation baking process, compiling all queued ability requests into a unified prediction buffer. */
	void BakeSimulation();

	/** Advances or rewinds the presage timeline to a specific timestamp for scrubbing and visualization. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Scrub To Time"), Category = "WolfCore|Presage")
	/**
	 * @param Time The target timestamp in seconds to scrub the prediction timeline to.
	 */
	void ScrubToTime(float Time);

	// ============================================================================================================================
	// Subsystem Lifecycle
	// ============================================================================================================================

	/** Initializes the subsystem when it is registered with the world, setting up internal state and tag references. */
	/**
	 * @param Collection Reference to the FSubsystemCollectionBase containing all initialized subsystems.
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Deinitializes the subsystem when it is being shut down, cleaning up prediction buffers and participant lists. */
	virtual void Deinitialize() override;

	/** Retrieves the singleton instance of the presage subsystem from the specified world context. */
	/**
	 * @param World Pointer to the UWorld used for subsystem lookup and context resolution.
	 * @return Pointer to the UPresageSubsystem, or nullptr if not found in the world.
	 */
	static UPresageSubsystem* Get(const UWorld* World);

	// ============================================================================================================================
	// Tickables
	// ============================================================================================================================

	/** Executes a single tick of the presage prediction loop, advancing simulation state by one frame. */
	/**
	 * @param DeltaTime The time elapsed since this tick was last executed in seconds.
	 */
	virtual void Tick(float DeltaTime) override;

	/** Returns the unique stat identifier used for profiling and debugging tickable subsystem performance. */
	TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UPresageSubsystem, STATGROUP_Tickables);
	}

	// ============================================================================================================================
	// Public API - Presage Flow Control
	// ============================================================================================================================

	/** Initiates the presage prediction loop by starting continuous temporal simulation and event processing. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Start Prediction Loop"), Category = "WolfCore|Presage")
	void StartLoop();

	/** Stops the presage prediction loop, halting temporal simulation and clearing active prediction buffers. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Stop Prediction Loop"), Category = "WolfCore|Presage")
	void StopLoop();

	// ============================================================================================================================
	// Public API - Ability Queue & Participants
	// ============================================================================================================================

	/** Queues a combat ability request for temporal prediction and adds it to the simulation processing pipeline. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Queue Ability Request"), Category = "WolfCore|Presage")
	/**
	 * @param Request The FPresageAbilityRequest containing ability parameters and temporal data for prediction.
	 */
	void QueueAbilityRequest(const FPresageAbilityRequest& Request);

	/** Sets the participant mode (turn-based or real-time) for a character during presage simulation. */
	/**
	 * @param Character Pointer to the AWolfCharacterBase whose participation mode is being configured.
	 * @param bIsTurnBased Boolean flag indicating whether this character uses turn-based prediction logic.
	 */
	void SetParticipantMode(AWolfCharacterBase* Character, bool bIsTurnBased);

	/** Removes a character from the presage participant list and clears its prediction data. */
	/**
	 * @param Character Pointer to the AWolfCharacterBase being removed from active participation.
	 */
	void RemoveParticipant(AWolfCharacterBase* Character);

	/** Array of predicted timeline events representing all scheduled combat interactions during simulation. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TArray<FPresageTimelineEvent> CurrentPredictedTimeline;

	// ============================================================================================================================
	// Protected - State
	// ============================================================================================================================

protected:
	/** Array of original character states captured before simulation begins for state restoration. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Presage")
	TArray<FActorSnapshot> OriginalCharacterStates;

	/** Flow time duration in seconds controlling the rate at which flow gauge accumulates during simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Presage", meta=(ClampMin = "0.0", UIMin = "0.0"))
	float FlowTime = 3.f;

	/** Array of queued ability requests awaiting processing by the presage prediction loop. */
	UPROPERTY()
	TArray<FPresageAbilityRequest> AbilityQueue;

	// ============================================================================================================================
	// Private - Internal State
	// ============================================================================================================================

private:
	/** Pointer to the singleton gameplay tags structure for framework-wide tag access during simulation. */
	const FWolfGameplayTags* WolfTags;

	/** Boolean flag indicating whether the presage prediction loop is currently active and processing ticks. */
	bool bLoopActive = false;

	/** Accumulated time in seconds tracking total elapsed time since the last frame boundary reset. */
	float AccumulatedTime = 0.f;

	/** Fixed prediction time step in seconds defining the duration between each simulation tick during presage. */
	const float PredictionTimeStep = 0.033f;

	/** Array of weak references to turn-based character participants awaiting their scheduled ability execution. */
	TArray<TWeakObjectPtr<AWolfCharacterBase>> TBParticipants;

	/** Array of weak references to real-time character participants actively processing during simulation ticks. */
	TArray<TWeakObjectPtr<AWolfCharacterBase>> RTParticipants;

	// ============================================================================================================================
	// Private - Event Gathering & Organization
	// ============================================================================================================================

	/** Gathers predicted combat events from all real-time participants and populates the provided event array. */
	/**
	 * @param Events Output reference to TArray<FPresageTimelineEvent> populated with gathered RT events.
	 */
	void GatherRTEvents(TArray<FPresageTimelineEvent>& Events);

	/** Gathers predicted combat events from all turn-based participants and populates the provided event array. */
	/**
	 * @param Events Output reference to TArray<FPresageTimelineEvent> populated with gathered TB events.
	 */
	void GatherTBEvents(TArray<FPresageTimelineEvent>& Events);

	/** Organizes predicted combat events by their scheduled impact time for temporal ordering and resolution. */
	/**
	 * @param Events Reference to the TArray<FPresageTimelineEvent> to be sorted chronologically.
	 */
	void OrganizeEventsByTime(TArray<FPresageTimelineEvent>& Events);

	// ============================================================================================================================
	// Private - Flow Timer & State Management
	// ============================================================================================================================

	/** Handles flow timer tick events, advancing the flow gauge accumulation and triggering state updates. */
	void OnFlowTimerTick();

	/** Captures snapshots of all participant characters' current states for subsequent simulation restoration. */
	void CharacterSnapshot();

	/** Restores original character states from captured snapshots after simulation completion or interruption. */
	void RevertCharacterStates();

	/** Updates the timeline prediction by processing queued events and advancing the temporal state forward. */
	void UpdateTimelinePrediction();

	/** Checks for future collision between two characters at a given time to prevent overlapping predictions. */
	/**
	 * @param Attacker Pointer to the attacking character being evaluated for collision prediction.
	 * @param Victim Pointer to the victim character receiving predicted attacks.
	 * @param FutureTime The target timestamp in seconds to evaluate potential collision overlap.
	 * @return True if a future collision is detected; false otherwise.
	 */
	static bool CheckFutureCollision(const AWolfCharacterBase* Attacker, const AWolfCharacterBase* Victim, float FutureTime);

	/** Calculates the predicted impact time from an ability sequence's combat periods for temporal prediction queries. */
	/**
	 * @param Sequence Reference to the TArray<FCombatPeriod> containing the ability period data.
	 * @return Float representing the calculated impact time in seconds.
	 */
	static float CalculateImpactFromSequence(const TArray<FCombatPeriod>& Sequence);
};