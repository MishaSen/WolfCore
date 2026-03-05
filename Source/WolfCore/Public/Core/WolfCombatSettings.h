#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WolfCombatSettings.generated.h"

class UGameplayEffect;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Wolf Combat Settings"))
class WOLFCORE_API UWolfCombatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Presage")
	TSoftClassPtr<UGameplayEffect> PresageEffectClass;

	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (Categories = "InputState"))
	TMap<FGameplayTag, float> ModeTimeDilationMap;
};
