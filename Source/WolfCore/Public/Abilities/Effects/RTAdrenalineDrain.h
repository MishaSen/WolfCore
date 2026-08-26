// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RTAdrenalineDrain.generated.h"

/**
 * Infinite periodic GameplayEffect that drains the owner's Adrenaline while the player is in RT
 * (ResourceLoop stage 4 — the spend side of the loop).
 *
 * Constructor-configured exactly like UPresageMode's pattern: infinite duration, periodic
 * (Period = 1s, bExecutePeriodicEffectOnApplication = false), a single Additive Adrenaline
 * modifier of -RTAdrenalineDrainPerSecond. Because GE modifier magnitudes are baked at
 * construction, the drain rate is read from UWolfCombatSettings::RTAdrenalineDrainPerSecond via
 * GetDefault at construction time — a config change requires a restart. That is an acceptable
 * limitation for a passive drain rate (revisit with a SetByCaller magnitude if it ever becomes
 * a live-tuning pain point).
 *
 * Applied/removed in UCombatModeSubsystem::ApplyModeToActor for the PLAYER only — present iff the
 * player's mode is RT (mirrors how ApplyModeToActor used to toggle the retired TB drain). The
 * drain ticks fine in RT because RT dilation is 1.0 (the frozen-periodic-GE problem was
 * TB-specific). PreAttributeChange already floors Adrenaline at 0, so no effect-removal at zero
 * is needed — a periodic -1 against a 0 floor is harmless.
 */
UCLASS()
class WOLFCORE_API URTAdrenalineDrain : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URTAdrenalineDrain();
};