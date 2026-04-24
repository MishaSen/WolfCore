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

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Snapshot")
	void CreateSnapshot(FActorSnapshot& NewSnapshot);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Snapshot")
	void RestoreSnapshot(const FActorSnapshot& StoredSnapshot);
};