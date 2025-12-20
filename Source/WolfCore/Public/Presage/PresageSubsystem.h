// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Snapshot.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "PresageSubsystem.generated.h"

class AWolfCharacterBase;
struct FPresageAbilityRequest;
struct FActorSnapshot;
struct FGameplayAbilitySpecHandle;
class UGameplayAbility;
class UAbilitySystemComponent;

USTRUCT(BlueprintType)
struct FPresageTimelineEvent
{
	GENERATED_BODY()
	
	FPresageTimelineEvent() = default;
	
	FPresageTimelineEvent(AActor* InInstigator, AActor* InTarget, float InTimeOffset, FGameplayTag InAbilityTag)
		: Instigator(InInstigator)
		, Target(InTarget)
		, TimeOffset(InTimeOffset)
		, AbilityTag(InAbilityTag)
	{}

	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	TObjectPtr<AActor> Instigator;

	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	TObjectPtr<AActor> Target;

	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	float TimeOffset;

	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	FGameplayTag AbilityTag;

};
/**
 *
 */
UCLASS()
class WOLFCORE_API UPresageSubsystem : public UTickableWorldSubsystem, public ISnapshot
{
	GENERATED_BODY()

public:
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
	bool bLoopActive = false;
	float AccumulatedTime = 0.f;

	void OnFlowTimerTick();
	void CharacterSnapshot();
	void RevertCharacterStates();
	void UpdateTimelinePrediction();
	bool CheckFutureCollision(AWolfCharacterBase* Attacker, AWolfCharacterBase* Victim, float FutureTime);
};
