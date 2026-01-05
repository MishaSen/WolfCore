// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/AbilitySystem/WolfAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Character/WolfCharacterBase.h"
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

	if (Attribute == GetAdrenalineAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UWolfAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const auto Attribute = Data.EvaluatedData.Attribute;
	auto* TargetActor = Data.Target.GetAvatarActor();
	const FString ActorName = TargetActor ? TargetActor->GetName() : TEXT("NULL");

	if (Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		WOLF_LOG(Log, TEXT("[%s] Health: %f"), *ActorName, GetHealth());

		if (GetHealth() <= 0.f)
		{
			if (auto* Victim = Cast<AWolfCharacterBase>(TargetActor)) Victim->Die();
		}
		return;
	}

	if (Attribute == GetAdrenalineAttribute())
	{
		SetAdrenaline(FMath::Max(GetAdrenaline(), 0.f));
		WOLF_LOG(Log, TEXT("[%s] Adrenaline: %f"), *ActorName, GetAdrenaline());
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
