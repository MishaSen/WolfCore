// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AbilityFrameData.generated.h"

/**
 * DataAsset defining timing windows for ability frames (startup, active, recovery).
 */
UCLASS()
class WOLFCORE_API UAbilityFrameData : public UDataAsset
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Timing Windows
	// ============================================================================================================================

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frame Data", meta = (ClampMin = "0"))
	float StartupTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frame Data", meta = (ClampMin = "0"))
	float ActiveTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frame Data", meta = (ClampMin = "0"))
	float RecoveryTime = 0.3f;

	// ============================================================================================================================
	// Helpers
	// ============================================================================================================================

	UFUNCTION(BlueprintPure, Category = "Frame Data")
	float GetTotalTime() const { return StartupTime + ActiveTime + RecoveryTime; }
};