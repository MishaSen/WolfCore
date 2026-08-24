// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ResourceGainEffect.generated.h"

/**
 * Single reusable instant GameplayEffect for applying any resource gain (ResourceLoop stage 1).
 * Two Additive modifiers — Gauge on SetByCaller tag `Data.FlowAmount`, Adrenaline on
 * `Data.AdrenalineAmount` — each inert (magnitude 0) when its tag is absent from the applied
 * spec. The gain site (UCombatModeSubsystem::ApplyResourceGainToPlayer) sets both SetByCaller
 * magnitudes on one spec and applies a single instance per hit. Applying through GAS keeps
 * clamping (PreAttributeChange), logging (PostGameplayEffectExecute), and — after ResourceLoop
 * stage 2 adds it — the max-clamp inside the normal pipeline rather than raw attribute pokes.
 */
UCLASS()
class WOLFCORE_API UResourceGainEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UResourceGainEffect();
};