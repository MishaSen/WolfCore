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

	UPROPERTY()
	float Timestamp = 0.f;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	float MontagePosition = 0.f;

	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> ActiveMontage = nullptr;

	UPROPERTY()
	float CurrentHealth = 0.f;
};

/**
 * Timeline track containing a sequence of character frames for visualization.
 */
USTRUCT(BlueprintType)
struct FCharacterTimelineTrack
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCharacterFrame> Frames;
};

/**
 * Event on the presage timeline representing a predicted combat interaction.
 */
USTRUCT(BlueprintType)
struct FPresageTimelineEvent
{
	GENERATED_BODY()

	FPresageTimelineEvent() = default;

	FPresageTimelineEvent(AActor* InAttacker, AActor* InVictim, float InTime, FGameplayTag InAbilityTag)
		: Attacker(InAttacker)
		, Victim(InVictim)
		, Time(InTime)
		, ResultTag(InAbilityTag)
	{}

	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	TObjectPtr<AActor> Attacker;

	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	TObjectPtr<AActor> Victim;

	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	float Time = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Presage")
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
	UPROPERTY(BlueprintReadOnly)
	TMap<AWolfCharacterBase*, FCharacterTimelineTrack> VisualTracks;

	void BakeSimulation();

	UFUNCTION(BlueprintCallable)
	void ScrubToTime(float Time);

	// ============================================================================================================================
	// Subsystem Lifecycle
	// ============================================================================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UPresageSubsystem* Get(const UWorld* World);

	// ============================================================================================================================
	// Tickables
	// ============================================================================================================================

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UPresageSubsystem, STATGROUP_Tickables);
	}

	// ============================================================================================================================
	// Public API - Presage Flow Control
	// ============================================================================================================================

	UFUNCTION(BlueprintCallable, Category = "Presage")
	void StartLoop();

	UFUNCTION(BlueprintCallable, Category = "Presage")
	void StopLoop();

	// ============================================================================================================================
	// Public API - Ability Queue & Participants
	// ============================================================================================================================

	UFUNCTION(BlueprintCallable, Category = "Presage")
	void QueueAbilityRequest(const FPresageAbilityRequest& Request);

	void SetParticipantMode(AWolfCharacterBase* Character, bool bIsTurnBased);
	void RemoveParticipant(AWolfCharacterBase* Character);

	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	TArray<FPresageTimelineEvent> CurrentPredictedTimeline;

	// ============================================================================================================================
	// Protected - State
	// ============================================================================================================================

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	TArray<FActorSnapshot> OriginalCharacterStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presage", meta=(ClampMin = "0.0", UIMin = "0.0"))
	float FlowTime = 3.f;

	UPROPERTY()
	TArray<FPresageAbilityRequest> AbilityQueue;

	// ============================================================================================================================
	// Private - Internal State
	// ============================================================================================================================

private:
	const FWolfGameplayTags* WolfTags;
	bool bLoopActive = false;
	float AccumulatedTime = 0.f;
	const float PredictionTimeStep = 0.033f;

	TArray<TWeakObjectPtr<AWolfCharacterBase>> TBParticipants;
	TArray<TWeakObjectPtr<AWolfCharacterBase>> RTParticipants;

	// ============================================================================================================================
	// Private - Event Gathering & Organization
	// ============================================================================================================================

	void GatherRTEvents(TArray<FPresageTimelineEvent>& Events);
	void GatherTBEvents(TArray<FPresageTimelineEvent>& Events);
	void OrganizeEventsByTime(TArray<FPresageTimelineEvent>& Events);

	// ============================================================================================================================
	// Private - Flow Timer & State Management
	// ============================================================================================================================

	void OnFlowTimerTick();
	void CharacterSnapshot();
	void RevertCharacterStates();
	void UpdateTimelinePrediction();
	static bool CheckFutureCollision(const AWolfCharacterBase* Attacker, const AWolfCharacterBase* Victim, float FutureTime);
	static float CalculateImpactFromSequence(const TArray<FCombatPeriod>& Sequence);
};