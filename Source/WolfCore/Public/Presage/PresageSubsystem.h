// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Snapshot.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Core/WolfGameplayTags.h"
#include "Subsystems/WorldSubsystem.h"

#include "PresageSubsystem.generated.h"

struct FCombatPeriod;
class AWolfCharacterBase;
struct FPresageAbilityRequest;
struct FActorSnapshot;
struct FGameplayAbilitySpecHandle;
class UGameplayAbility;
class UAbilitySystemComponent;

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

USTRUCT(BlueprintType)
struct FCharacterTimelineTrack
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCharacterFrame> Frames;
};

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
 *
 */
UCLASS()
class WOLFCORE_API UPresageSubsystem : public UTickableWorldSubsystem, public ISnapshot
{
	GENERATED_BODY()

public:
	// --- Database of Predictions ---
	UPROPERTY(BlueprintReadOnly)
	TMap<AWolfCharacterBase*, FCharacterTimelineTrack> VisualTracks;

	void BakeSimulation();

	UFUNCTION(BlueprintCallable)
	void ScrubToTime(float Time);
	
	void OnModeSwitchEventReceived(FGameplayTag GameplayTag, const FGameplayEventData* GameplayEventData);
	void BindToModeSwitchEvent();
	// --- Subsystem Lifecycle ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Global Subsystem Accessor ---
	static UPresageSubsystem* Get(const UWorld* World);
	
	// --- Tickables ---
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UPresageSubsystem, STATGROUP_Tickables);
	}

	// --- Presage Flow Functions ---
	UFUNCTION(BlueprintCallable, Category = "Presage")
	void StartLoop();

	UFUNCTION(BlueprintCallable, Category = "Presage")
	void StopLoop();
	
	// --- Ability Queue --- 
	UFUNCTION(BlueprintCallable, Category = "Presage")
	void QueueAbilityRequest(const FPresageAbilityRequest& Request);

	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	TArray<FPresageTimelineEvent> CurrentPredictedTimeline;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	TArray<FActorSnapshot> OriginalCharacterStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presage", meta=(ClampMin = "0.0", UIMin = "0.0"))
	float FlowTime = 3.f;

	UPROPERTY()
	TArray<FPresageAbilityRequest> AbilityQueue;

private:
	const FWolfGameplayTags* WolfTags;
	bool bLoopActive = false;
	float AccumulatedTime = 0.f;
	const float PredictionTimeStep = 0.033f;

	TArray<TWeakObjectPtr<AWolfCharacterBase>> TBParticipants;
	TArray<TWeakObjectPtr<AWolfCharacterBase>> RTParticipants;

	void GatherRTEvents(TArray<FPresageTimelineEvent>& Events);
	void GatherTBEvents(TArray<FPresageTimelineEvent>& Events);
	void OrganizeEventsByTime(TArray<FPresageTimelineEvent>& Events);

	void OnFlowTimerTick();
	void CharacterSnapshot();
	void RevertCharacterStates();
	void UpdateTimelinePrediction();
	static bool CheckFutureCollision(const AWolfCharacterBase* Attacker, const AWolfCharacterBase* Victim, float FutureTime);
	static float CalculateImpactFromSequence(const TArray<FCombatPeriod>& Sequence);
};
