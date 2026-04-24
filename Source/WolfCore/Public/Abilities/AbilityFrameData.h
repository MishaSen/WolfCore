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
	/** Duration of the startup frame in seconds. This is the windup period before an attack connects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|Frame Data", meta = (ClampMin = "0"))
	float StartupTime = 0.1f;

	/** Duration of the active frame in seconds. This is when the attack hits and damage occurs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|Frame Data", meta = (ClampMin = "0"))
	float ActiveTime = 0.2f;

	/** Duration of the recovery frame in seconds. This is the post-attack winddown period. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|Frame Data", meta = (ClampMin = "0"))
	float RecoveryTime = 0.3f;

	// ============================================================================================================================
	// Helpers
	// ============================================================================================================================

	/** Returns the total combined duration of all frame windows (startup + active + recovery). */
	UFUNCTION(BlueprintPure, Meta = (DisplayName = "Get Total Frame Time"), Category = "WolfCore|Frame Data")
	/**
	 * @return Float representing the total frame time in seconds.
	 */
	float GetTotalTime() const { return StartupTime + ActiveTime + RecoveryTime; }
};