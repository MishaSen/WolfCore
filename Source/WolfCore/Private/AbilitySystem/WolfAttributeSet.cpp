// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/AbilitySystem/WolfAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Core/WolfGameplayTags.h"

void UWolfAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UWolfAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));

		if (GetHealth() <= 0.f)
		{
			auto* Killer = Data.EffectSpec.GetContext().GetEffectCauser();
			/*
			 * Consider adding interface faction Die()
			 * So far the plan is for all characters to inherit from WolfCharacterBase, so it's fine for now
			 */
		}
	}
}

void UWolfAttributeSet::PostInitProperties()
{
	Super::PostInitProperties();

	if (TagToAttributeMap.IsEmpty())
	{
		TagToAttributeMap.Add(FWolfGameplayTags::Get().Attribute_Health, &GetHealthAttribute);
		TagToAttributeMap.Add(FWolfGameplayTags::Get().Attribute_MaxHealth, &GetMaxHealthAttribute);
		TagToAttributeMap.Add(FWolfGameplayTags::Get().Attribute_FlowGauge, &GetFlowGaugeAttribute);
		TagToAttributeMap.Add(FWolfGameplayTags::Get().Attribute_Adrenaline, &GetAdrenalineAttribute);
	}
}

FGameplayAttribute UWolfAttributeSet::GetAttributeByTag(const FGameplayTag& Tag)
{
	if (TagToAttributeMap.Contains(Tag))
	{
		return TagToAttributeMap[Tag]();
	}
	return FGameplayAttribute();
}