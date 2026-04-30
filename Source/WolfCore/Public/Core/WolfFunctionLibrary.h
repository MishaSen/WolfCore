#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WolfFunctionLibrary.generated.h"

/**
 * Utility function library providing C++-only helper functions.
 */
UCLASS()
class WOLFCORE_API UWolfFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	// ============================================================================================================================
	// World Subsystem Accessors
	// ============================================================================================================================

public:
	/** Retrieves a world subsystem of the specified type from the UObject's context. Template helper for internal C++ use only. */
	/**
	 * @tparam T The subclass of UWorldSubsystem to retrieve.
	 * @param WorldContextObject Pointer to any UObject that can provide world context.
	 * @return Pointer to the requested subsystem, or nullptr if the world could not be obtained.
	 */
	template<typename T>
	static T* GetWorldSubsystem(const UObject* WorldContextObject)
	{
		if (const auto* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			return World->GetSubsystem<T>();
		}
		return nullptr;
	}

	// ============================================================================================================================
	// Presage Projection Math (Decoupled from Character)
	// ============================================================================================================================

public:
	/**
	 * Projects a transform based on the current animation montage at a given scrub position.
	 * Uses the AnimInstance to sample bone transforms at the specified time offset.
	 * 
	 * @param AnimInstance The cached AnimInstance for the character (must not be null).
	 * @param Montage The active AnimMontage being simulated (may be null; falls back to base pose if absent).
	 * @param ScrubPosition The playback position in seconds within the montage.
	 * @param BaseTransform The root transform to apply as the base offset for projection.
	 * @return FTransform representing the projected world-space transform at the specified animation position.
	 */
	UFUNCTION(BlueprintPure, Category = "Wolf|Presage|Projection")
	static FTransform GetProjectedTransform(
		const UAnimInstance* AnimInstance,
		UAnimMontage* Montage,
		float ScrubPosition,
		const FTransform& BaseTransform);

	/**
	 * Extracts root motion delta from an AnimMontage at a specific time position.
	 * Useful for calculating velocity/acceleration deltas during presage simulation steps.
	 * 
	 * @param Montage The AnimMontage to extract root motion from (must not be null).
	 * @param TimePosition The current playback position in seconds within the montage.
	 * @return FTransform representing the extracted root motion velocity/delta vector.
	 */
	UFUNCTION(BlueprintPure, Category = "Wolf|Presage|RootMotion")
	static FTransform ExtractRootMotionAtTime(
		const UAnimMontage* Montage,
		float TimePosition);
};