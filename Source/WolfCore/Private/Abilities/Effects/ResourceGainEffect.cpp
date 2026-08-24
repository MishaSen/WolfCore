// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/Effects/ResourceGainEffect.h"

#include "AbilitySystem/WolfAttributeSet.h"
#include "Core/WolfGameplayTags.h"
#include "GameplayEffect.h"

UResourceGainEffect::UResourceGainEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat FlowSetByCaller;
	FlowSetByCaller.DataTag = FWolfGameplayTags::Get().Data_FlowAmount;

	// Flow Gauge modifier — SetByCaller magnitude on Data.FlowAmount, read at apply time.
	FGameplayModifierInfo FlowGaugeModifier;
	FlowGaugeModifier.Attribute = UWolfAttributeSet::GetFlowGaugeAttribute();
	FlowGaugeModifier.ModifierOp = EGameplayModOp::Additive;
	FlowGaugeModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FlowSetByCaller);
	Modifiers.Add(FlowGaugeModifier);

	FSetByCallerFloat AdrenalineSetByCaller;
	AdrenalineSetByCaller.DataTag = FWolfGameplayTags::Get().Data_AdrenalineAmount;

	// Adrenaline modifier — SetByCaller magnitude on Data.AdrenalineAmount, read at apply time.
	FGameplayModifierInfo AdrenalineModifier;
	AdrenalineModifier.Attribute = UWolfAttributeSet::GetAdrenalineAttribute();
	AdrenalineModifier.ModifierOp = EGameplayModOp::Additive;
	AdrenalineModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(AdrenalineSetByCaller);
	Modifiers.Add(AdrenalineModifier);
}