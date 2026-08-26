// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/Effects/RTAdrenalineDrain.h"

#include "AbilitySystem/WolfAttributeSet.h"
#include "Core/WolfCombatSettings.h"
#include "GameplayEffect.h"

URTAdrenalineDrain::URTAdrenalineDrain()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Modifier magnitudes are baked at construction, so read the drain rate from project config
	// here. GetDefault<UWolfCombatSettings> returns the CDO; a config edit requires a restart —
	// acceptable for a passive drain rate (see header doc; revisit with SetByCaller if needed).
	const float DrainPerSecond = GetDefault<UWolfCombatSettings>()
		? GetDefault<UWolfCombatSettings>()->RTAdrenalineDrainPerSecond
		: 1.f;

	FGameplayModifierInfo AdrenalineDrain;
	AdrenalineDrain.Attribute = UWolfAttributeSet::GetAdrenalineAttribute();
	AdrenalineDrain.ModifierOp = EGameplayModOp::Additive;
	AdrenalineDrain.ModifierMagnitude = FScalableFloat(-DrainPerSecond);
	Modifiers.Add(AdrenalineDrain);

	Period = 1.f;
	bExecutePeriodicEffectOnApplication = false;
}