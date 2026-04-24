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
};