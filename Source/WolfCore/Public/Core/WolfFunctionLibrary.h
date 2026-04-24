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