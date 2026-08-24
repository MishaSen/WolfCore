// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/AbilitySystem/WolfAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Character/WolfCharacterBase.h"
#include "Core/WolfCombatSettings.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "GameplayTagContainer.h"

// ResourceLoop stage 2 — warns exactly once per process when the FlowGauge soft cap is
// unconfigured (the player's UCharacterStatConfig has no MaxFlowGauge default yet), so the
// guarded clamp never silently zeroes Flow gains without leaving a discoverable trace.
static void FLOW_WarnMaxFlowUnconfigured()
{
	static bool bWarned = false;
	if (bWarned) return;
	bWarned = true;
	WOLF_WARN(TEXT("MaxFlowGauge is 0 — FlowGauge upper clamp skipped. Add a MaxFlowGauge default to the player's UCharacterStatConfig."));
}

TMap<FGameplayTag, TFunction<FGameplayAttribute()>> UWolfAttributeSet::TagToAttributeMap;

void UWolfAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}

	if (Attribute == GetMaxFlowGaugeAttribute())
	{
		// Hard ceiling on the soft cap itself (ResourceLoop stage 2): progression upgrades
		// MaxFlowGauge, but it can never exceed the vision's hard ceiling. Read from the
		// project's UWolfCombatSettings (designer constant), not from live ASC state.
		const auto* Settings = GetDefault<UWolfCombatSettings>();
		NewValue = FMath::Clamp(NewValue, 0.f, Settings ? Settings->FlowHardCeiling : 0.f);
	}

	if (Attribute == GetAdrenalineAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	
	if (Attribute == GetFlowGaugeAttribute())
	{
		const float MaxFlow = GetMaxFlowGauge();
		if (MaxFlow > 0.f)
		{
			// ResourceLoop stage 2 — replaces the floor-only clamp with the soft cap: Flow stays in
			// [0, GetMaxFlowGauge()]. This is what makes the cap a cap (upgradeable over time).
			NewValue = FMath::Clamp(NewValue, 0.f, MaxFlow);
		}
		else
		{
			// Degenerate-but-expected case: MaxFlowGauge is 0 because StatConfig hasn't authored a
			// default yet. Keep the floor-only clamp and warn ONCE (see FLOW_WarnMaxFlowUnconfigured)
			// instead of silently zeroing every Flow gain on an unconfigured character.
			NewValue = FMath::Max(NewValue, 0.f);
			FLOW_WarnMaxFlowUnconfigured();
		}
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

	if (Attribute == GetMaxFlowGaugeAttribute())
	{
		// Mirror the hard-ceiling clamp (see PreAttributeChange) for GE-driven cap changes — an
		// upgrade effect can move the cap up, but never past the configured ceiling.
		const auto* Settings = GetDefault<UWolfCombatSettings>();
		SetMaxFlowGauge(FMath::Clamp(GetMaxFlowGauge(), 0.f, Settings ? Settings->FlowHardCeiling : 0.f));
		return;
	}

	if (Attribute == GetAdrenalineAttribute())
	{
		SetAdrenaline(FMath::Max(GetAdrenaline(), 0.f));
		WOLF_LOG(Log, TEXT("[%s] Adrenaline: %f"), *ActorName, GetAdrenaline());
	}
	
	if (Attribute == GetFlowGaugeAttribute())
	{
		const float MaxFlow = GetMaxFlowGauge();
		if (MaxFlow > 0.f)
		{
			SetFlowGauge(FMath::Clamp(GetFlowGauge(), 0.f, MaxFlow));
		}
		else
		{
			SetFlowGauge(FMath::Max(GetFlowGauge(), 0.f));
			FLOW_WarnMaxFlowUnconfigured();
		}
		WOLF_LOG(Log, TEXT("[%s] Flow Gauge: %f"), *ActorName, GetFlowGauge());
	}
}

FGameplayAttribute UWolfAttributeSet::GetAttributeByTag(const FGameplayTag& Tag)
{
	if (TagToAttributeMap.IsEmpty())
	{
		const auto& Tags = FWolfGameplayTags::Get();

		TagToAttributeMap.Add(Tags.Attribute_Health, []() { return GetHealthAttribute(); });
		TagToAttributeMap.Add(Tags.Attribute_MaxHealth, []() { return GetMaxHealthAttribute(); });
		TagToAttributeMap.Add(Tags.Attribute_MaxFlowGauge, []() { return GetMaxFlowGaugeAttribute(); });
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
