// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/Effects/PresageMode.h"

#include "AbilitySystem/WolfAttributeSetBase.h"
#include "AbilitySystem/WolfPlayerAttributeSet.h"
#include "Core/WolfGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UPresageMode::UPresageMode()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo FlowGaugeDrain;
	FlowGaugeDrain.Attribute = UWolfPlayerAttributeSet::GetFlowGaugeAttribute();
	FlowGaugeDrain.ModifierOp = EGameplayModOp::Additive;
	FlowGaugeDrain.ModifierMagnitude = FScalableFloat(-1.f);
	Modifiers.Add(FlowGaugeDrain);

	Period = 1.f;
	bExecutePeriodicEffectOnApplication = false;
}
