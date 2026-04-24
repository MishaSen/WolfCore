// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "Presage/ActorSnapshot.h"
#include "Presage/Snapshot.h"
#include "WolfPresageComponent.generated.h"

class AWolfCharacterBase;
class UAnimInstance;
class UCombatModeSubsystem;
class UCharacterMovementComponent;
class UWolfAbilityComponent;
class UWolfAbilitySystemComponent;

/**
 * Component responsible for presage simulation, snapshot capture/restoration, and future-state prediction.
 * Handles physics simulation, animation scrubbing, and Gameplay Ability System state during temporal prediction.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WOLFCORE_API UWolfPresageComponent : public UActorComponent, public ISnapshot
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	UWolfPresageComponent();

	virtual void BeginPlay() override;

	// ============================================================================================================================
	// Public API - Prediction Buffer
	// ============================================================================================================================

	void ClearPredictionBuffer(float MaxDuration);
	const FActorSnapshot* GetSnapshotAtTime(float RelativeTime) const;

	// ============================================================================================================================
	// Public API - Simulation Control
	// ============================================================================================================================

	void SetIsSimulating(bool bState) { bIsSimulating = bState; }
	void SetSimulationTransform(const FTransform& NewTransform) { SimulationTransform = NewTransform; }
	virtual void SimulateTick(float Step);

	// ============================================================================================================================
	// Public API - Snapshot Interface
	// ============================================================================================================================

	virtual void CreateSnapshot_Implementation(FActorSnapshot& OutSnapshot) override;
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot) override;

	// ============================================================================================================================
	// Simulation State
	// ============================================================================================================================

public:
	float SimPeriodTime = 0.f;

	// ============================================================================================================================
	// Private - Physics Simulation
	// ============================================================================================================================

private:
	void SimulatePhysicsStep(float Step);
	FVector GetSimulatedVelocity(FVector& Destination) const;
	void ResolveMovementWithCollision(const FVector& Start, const FVector& End, FVector& Delta);
	void SnapshotPhysics(FActorSnapshot& Snapshot) const;
	void RestorePhysics(const FActorSnapshot& Snapshot);

	// ============================================================================================================================
	// Private - Animation Simulation
	// ============================================================================================================================

	void SimulateAnimationStep(float DeltaTime);
	void SnapshotAnim(FActorSnapshot& Snapshot) const;
	void RestoreAnim(const FActorSnapshot& Snapshot);

	// ============================================================================================================================
	// Private - GAS State Management
	// ============================================================================================================================

	void SnapshotGAS(FActorSnapshot& Snapshot) const;
	void RestoreGAS(const FActorSnapshot& Snapshot);

	// ============================================================================================================================
	// Private - Inline Accessors
	// ============================================================================================================================

	FORCEINLINE FVector GetOwnerLocation() const;
	FORCEINLINE FRotator GetOwnerRotation() const;
	FORCEINLINE FVector GetSimLocation() const;
	FORCEINLINE FRotator GetSimRotation() const;

	FORCEINLINE UCharacterMovementComponent* GetMoveComp() const;
	FORCEINLINE UWolfAbilitySystemComponent* GetASC() const;
	FORCEINLINE UAnimInstance* GetAnimInst() const;
	FORCEINLINE UAnimMontage* GetCurrentMontage() const;
	FORCEINLINE UCombatModeSubsystem* GetCMS() const;

	// ============================================================================================================================
	// Internal State
	// ============================================================================================================================

	bool bIsRestoringSnapshot = false;
	bool bIsSimulating = false;

	FTransform SimulationTransform;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TArray<FActorSnapshot> PredictionBuffer;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TObjectPtr<AWolfCharacterBase> CharacterOwner;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TObjectPtr<UWolfAbilityComponent> AbilityControl;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TObjectPtr<UWolfAbilitySystemComponent> CachedASC;
};