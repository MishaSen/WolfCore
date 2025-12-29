// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/AbilitySystem/WolfAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"

TMap<FGameplayTag, TFunction<FGameplayAttribute()>> UWolfAttributeSet::TagToAttributeMap;

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

FGameplayAttribute UWolfAttributeSet::GetAttributeByTag(const FGameplayTag& Tag)
{
	if (TagToAttributeMap.IsEmpty())
	{
		const auto& Tags = FWolfGameplayTags::Get();

		TagToAttributeMap.Add(Tags.Attribute_Health, []() { return GetHealthAttribute(); });
		TagToAttributeMap.Add(Tags.Attribute_MaxHealth, []() { return GetMaxHealthAttribute(); });
		TagToAttributeMap.Add(Tags.Attribute_FlowGauge, []() { return GetFlowGaugeAttribute(); });
		TagToAttributeMap.Add(Tags.Attribute_Adrenaline, []() { return GetAdrenalineAttribute(); });

		WOLF_LOG(Log, TEXT("Static Tag Map initialized"));
	}
	
	if (TagToAttributeMap.Contains(Tag))
	{
		return TagToAttributeMap[Tag]();
	}
	return FGameplayAttribute();
}