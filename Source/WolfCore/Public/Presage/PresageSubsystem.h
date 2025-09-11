// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "PresageSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransitionToTB, bool, bIsEnteringTB);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransitionToRT, bool, bIsEnteringRT);

struct FPresageAbilityRequest;
struct FActorState;
struct FGameplayAbilitySpecHandle;
class UGameplayAbility;
class UAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class WOLFCORE_API UPresageSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Subsystem Lifecyle ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Global Subsystem Accessor ---
	static UPresageSubsystem* Get(UWorld* World);
	
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

	// -- Delegate Broadcasts --
	UPROPERTY(BlueprintAssignable)
	FOnTransitionToTB OnTransitionToTB;

	UPROPERTY(BlueprintAssignable)
	FOnTransitionToRT OnTransitionToRT;
	
	// --- Ability Queue --- 
	UFUNCTION(BlueprintCallable, Category = "Presage")
	void QueueAbilityRequest(const FPresageAbilityRequest& Request);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Presage")
	TArray<FActorState> OriginalCharacterStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presage", meta=(ClampMin = "0.0", UIMin = "0.0"))
	float FlowTime = 3.f;

	UPROPERTY()
	TArray<FPresageAbilityRequest> AbilityQueue;

private:
	bool bLoopActive = false;
	float AccumulatedTime = 0.f;

	void OnFlowTimerTick();
	void CaptureCharacterStates();
	void RevertCharacterStates();
};
