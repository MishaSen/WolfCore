#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "CombatModeListener.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UCombatModeListener : public UInterface
{
	GENERATED_BODY()
};

class WOLFCORE_API ICombatModeListener
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintNativeEvent, Category = "Combat")
	void OnCombatModeChanged(FGameplayTag NewMode);
};