// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Presage/Snapshot.h"
#include "WolfSnapshotComponent.generated.h"


class UCharacterMovementComponent;
class UWolfAbilitySystemComponent;
class UWolfAbilityComponent;
class AWolfCharacterBase;
class UAnimInstance;
class UAnimMontage;
class UCombatModeSubsystem;

UCLASS(ClassGroup=(WolfCore), meta=(BlueprintSpawnableComponent))
class WOLFCORE_API UWolfSnapshotComponent : public UActorComponent, public ISnapshot
{
	GENERATED_BODY()

public:
	/** Default constructor for UWolfSnapshotComponent. */
	UWolfSnapshotComponent();

	/** Called at runtime when the component is ready to begin functioning. Caches owner references. */
	virtual void BeginPlay() override;

	// ============================================================================================================================
	// Public API - Snapshot Interface (ISnapshot)
	// ============================================================================================================================

public:
	/** Creates a full actor snapshot representing the current state for temporal prediction storage. Implements ISnapshot. */
	virtual void CreateSnapshot_Implementation(FActorSnapshot& OutSnapshot) override;

	/** Restores actor state from a previously captured snapshot, reversing temporal prediction changes. Implements ISnapshot. */
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot) override;

	// ============================================================================================================================
	// Public API - Accessors
	// ============================================================================================================================

public:
	/** Returns the cached reference to the owner character. */
	FORCEINLINE AWolfCharacterBase* GetOwnerCharacter() const { return OwnerCharacter; }

	/** Returns the cached Ability System Component for direct GAS interactions during snapshot operations. */
	FORCEINLINE UWolfAbilitySystemComponent* GetASC() const { return CachedASC; }

	/** Sets whether temporal simulation is currently active for this component. */
	void SetIsSimulating(bool bState) { bIsSimulating = bState; }

	/** Overrides the simulation transform to a specific world-space transform value. */
	void SetSimulationTransform(const FTransform& NewTransform) { SimulationTransform = NewTransform; }

	// ============================================================================================================================
	// Private - Snapshot Operations (Physics / Animation / GAS)
	// ============================================================================================================================

private:
	/** Captures physics state (location, velocity, collision) into a snapshot for temporal prediction storage. */
	void SnapshotPhysics(FActorSnapshot& Snapshot) const;

	/** Restores physics state from a previously captured snapshot, reversing simulation changes. */
	void RestorePhysics(const FActorSnapshot& Snapshot);

	/** Captures animation state (montage position, playback rate, blend layers) into a snapshot. */
	void SnapshotAnim(FActorSnapshot& Snapshot) const;

	/** Restores animation state from a previously captured snapshot, reversing simulation changes. */
	void RestoreAnim(const FActorSnapshot& Snapshot);

	/** Captures Gameplay Ability System state (attributes, active effects, cooldowns) into a snapshot. */
	void SnapshotGAS(FActorSnapshot& Snapshot) const;

	/** Restores Gameplay Ability System state from a previously captured snapshot. */
	void RestoreGAS(const FActorSnapshot& Snapshot);

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
	FORCEINLINE UCharacterMovementComponent* GetMoveComp() const { return CachedMoveComp; }

	/** Provides fast access to the cached animation instance for animation scrubbing operations. */
	FORCEINLINE UAnimInstance* GetAnimInst() const;

	/** Provides fast access to the current active animation montage being simulated. */
	FORCEINLINE UAnimMontage* GetCurrentMontage() const;

	/** Provides fast access to the Combat Mode subsystem singleton for combat mode queries during simulation. */
	FORCEINLINE UCombatModeSubsystem* GetCMS() const;

	// ============================================================================================================================
	// Internal State
	// ============================================================================================================================

private:
	mutable int32 SnapshotIndex = 0;
	
	/** Flag indicating whether the component is currently in a snapshot restoration sequence. */
	bool bIsRestoringSnapshot = false;

	/** Flag indicating whether temporal simulation is currently active for this component. */
	bool bIsSimulating = false;

	/** The base transform representing the simulated world-space position during prediction. */
	FTransform SimulationTransform;

	/** Strong reference to the owner character actor that owns this snapshot component. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "WolfCore|Internal")
	TObjectPtr<AWolfCharacterBase> OwnerCharacter;

	/** Strong reference to the ability control component for ability state management during simulation. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "WolfCore|Internal")
	TObjectPtr<UWolfAbilityComponent> AbilityControl;

	/** Cached reference to the Ability System Component for performance optimization during snapshot operations. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "WolfCore|Internal")
	TObjectPtr<UWolfAbilitySystemComponent> CachedASC;

	/** Cached reference to the Character Movement Component for physics state capture/restore. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "WolfCore|Internal")
	TObjectPtr<UCharacterMovementComponent> CachedMoveComp;
};
