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
class WOLFCORE_API UWolfPresageComponent : public UActorComponent
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	/** Default constructor for UWolfPresageComponent. */
	UWolfPresageComponent();

	/** Called at runtime when the component is ready to begin functioning. Overrides UActorComponent::BeginPlay(). */
	virtual void BeginPlay() override;

	// ============================================================================================================================
	// Public API - Prediction Buffer
	// ============================================================================================================================

	/** Clears all snapshots in the prediction buffer and resets the simulation timeline. */
	void ClearPredictionBuffer(float MaxDuration);

	/** Retrieves a snapshot from the prediction buffer at a given relative time offset. */
	/**
	 * @param RelativeTime Time delta to query against the prediction buffer.
	 * @return Pointer to the FActorSnapshot at the specified time, or nullptr if not found.
	 */
	const FActorSnapshot* GetSnapshotAtTime(float RelativeTime) const;

	// ============================================================================================================================
	// Public API - Simulation Control
	// ============================================================================================================================

	/** Sets whether temporal simulation is currently active for this component. */
	void SetIsSimulating(bool bState) { bIsSimulating = bState; }

	/** Overrides the simulation transform to a specific world-space transform value. */
	/**
	 * @param NewTransform The FTransform to set as the new simulation base transform.
	 */
	void SetSimulationTransform(const FTransform& NewTransform) { SimulationTransform = NewTransform; }

	/** Executes a single simulation tick step, advancing the temporal prediction state by one frame. */
	/**
	 * @param Step The time delta in seconds for this simulation tick step.
	 */
	virtual void SimulateTick(float Step);

	// Simulation State
	// ============================================================================================================================

public:
	/** Time delta between each simulation tick step during temporal prediction. */
	float SimPeriodTime = 0.f;

	// ============================================================================================================================
	// Private - Physics Simulation
	// ============================================================================================================================

private:
	/** Executes a single physics simulation step, updating character movement and collision state. */
	/**
	 * @param Step The time delta in seconds for this physics step.
	 */
	void SimulatePhysicsStep(float Step);

	/** Computes the simulated velocity vector based on destination position and current state. */
	/**
	 * @param Destination The target destination FVector for velocity calculation.
	 * @return FVector representing the computed simulated velocity.
	 */
	FVector GetSimulatedVelocity(FVector& Destination) const;

	/** Resolves movement collision between start and end positions, adjusting the delta accordingly. */
	/**
	 * @param Start The starting FVector position for collision resolution.
	 * @param End The ending FVector position for collision resolution.
	 * @param Delta Output parameter for the adjusted movement delta after collision resolution.
	 */
	void ResolveMovementWithCollision(const FVector& Start, const FVector& End, FVector& Delta);

	// ============================================================================================================================
	// Private - Animation Simulation
	// ============================================================================================================================

	/** Executes a single animation simulation step, advancing animation state and montage scrubbing. */
	/**
	 * @param DeltaTime The time delta in seconds for this animation step.
	 */
	void SimulateAnimationStep(float DeltaTime);

	// ============================================================================================================================
	// Private - Inline Accessors
	// ============================================================================================================================

	/** Provides fast access to the owner's current world-space location. */
	FORCEINLINE FVector GetOwnerLocation() const;

	/** Provides fast access to the owner's current rotation in FRotator format. */
	FORCEINLINE FRotator GetOwnerRotation() const;

	/** Provides fast access to the simulated location for temporal prediction queries. */
	FORCEINLINE FVector GetSimLocation() const;

	/** Provides fast access to the simulated rotation for temporal prediction queries. */
	FORCEINLINE FRotator GetSimRotation() const;

	/** Provides fast access to the character's movement component for simulation state management. */
	FORCEINLINE UCharacterMovementComponent* GetMoveComp() const;

	/** Provides fast access to the Ability System Component (ASC) for GAS interactions during simulation. */
	FORCEINLINE UWolfAbilitySystemComponent* GetASC() const;

	/** Provides fast access to the cached animation instance for animation scrubbing operations. */
	FORCEINLINE UAnimInstance* GetAnimInst() const;

	/** Provides fast access to the current active animation montage being simulated. */
	FORCEINLINE UAnimMontage* GetCurrentMontage() const;

	/** Provides fast access to the Combat Mode subsystem singleton for combat mode queries during simulation. */
	FORCEINLINE UCombatModeSubsystem* GetCMS() const;

	/** Provides fast access to the Snapshot component for state capture during simulation. */
	class UWolfSnapshotComponent* GetSnapshotControl() const;

	// ============================================================================================================================
	// Internal State
	// ============================================================================================================================

	/** Flag indicating whether temporal simulation is currently active for this component. */
	bool bIsSimulating = false;

	/** The base transform representing the simulated world-space position during prediction. */
	FTransform SimulationTransform;

	/** Array of snapshots capturing the prediction buffer for temporal state queries and visualization. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "WolfCore|Internal")
	TArray<FActorSnapshot> PredictionBuffer;

	/** Strong reference to the owning character actor that controls this presage component. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "WolfCore|Internal")
	TObjectPtr<AWolfCharacterBase> CharacterOwner;

	/** Strong reference to the ability control component for ability state management during simulation. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "WolfCore|Internal")
	TObjectPtr<UWolfAbilityComponent> AbilityControl;

	/** Cached reference to the Ability System Component (ASC) for performance optimization during simulation ticks. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "WolfCore|Internal")
	TObjectPtr<UWolfAbilitySystemComponent> CachedASC;
};