// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Snapshot.generated.h"

struct FActorSnapshot;

// ============================================================================================================================
// Interface Object
// ============================================================================================================================

/**
 * Interface object for snapshot functionality. Implement this interface to support state capture and restoration.
 */
UINTERFACE(MinimalAPI)
class USnapshot : public UInterface
{
	GENERATED_BODY()
};

// ============================================================================================================================
// Implementation Interface
// ============================================================================================================================

/**
 * Interface for capturing and restoring actor snapshots during presage simulation.
 * Implementing classes must provide CreateSnapshot and RestoreSnapshot events.
 */
class WOLFCORE_API ISnapshot
{
	GENERATED_BODY()

public:
	// ============================================================================================================================
	// Snapshot Operations
	// ============================================================================================================================

	/** Creates a full actor snapshot representing the current state for temporal prediction storage. BlueprintNativeEvent. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WolfCore|Snapshot")
	/**
	 * @param NewSnapshot Reference to the FActorSnapshot to populate with current state data.
	 */
	void CreateSnapshot(FActorSnapshot& NewSnapshot);

	/** Restores actor state from a previously captured snapshot, reversing temporal prediction changes. BlueprintNativeEvent. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WolfCore|Snapshot")
	/**
	 * @param StoredSnapshot The FActorSnapshot containing the state to restore.
	 */
	void RestoreSnapshot(const FActorSnapshot& StoredSnapshot);
};