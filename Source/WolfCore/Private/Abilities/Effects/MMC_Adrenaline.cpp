#include "../../../Public/Abilities/Effects/MMC_Adrenaline.h"

#include "Core/WolfGameplayTags.h"

float UMMC_Adrenaline::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const auto AmountTag = FWolfGameplayTags::Get().Data_Amount;
	const auto BaseValue = Spec.GetSetByCallerMagnitude(AmountTag, 0.f);

	return BaseValue;
}