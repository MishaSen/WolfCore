// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Presage/Snapshot.h"
#include "WolfSnapshotComponent.generated.h"


class UCharacterMovementComponent;
class UAbilitySystemComponent;
class AWolfCharacterBase;

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
	FORCEINLINE UAbilitySystemComponent* GetCachedASC() const { return CachedASC; }

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
    // Private - Cached References (Populated at BeginPlay)
    // ============================================================================================================================
    
private:
	/** Strong reference to the owner character actor that owns this snapshot component. */
	TObjectPtr<AWolfCharacterBase> OwnerCharacter;

	/** Cached reference to the Ability System Component for performance optimization during snapshot operations. */
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	/** Cached reference to the Character Movement Component for physics state capture/restore. */
	TObjectPtr<UCharacterMovementComponent> CachedMoveComp;
};