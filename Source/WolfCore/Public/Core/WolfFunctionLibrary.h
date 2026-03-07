#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "WolfFunctionLibrary.generated.h"

UCLASS()
class WOLFCORE_API UWolfFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

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