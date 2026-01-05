#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Adrenaline.generated.h"

UCLASS()
class WOLFCORE_API UMMC_Adrenaline : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};
