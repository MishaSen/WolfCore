// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UBaseCombatAbility;

/**
 * Static utility struct for advancing ability periods during simulation.
 * Decouples ability lifecycle logic from the presage simulation component.
 */
struct WOLFCORE_API FAbilityPeriodAdvancer
{
	/**
	 * Advances the period of an active combat ability by the given elapsed time.
	 * 
	 * Evaluates whether the current period's duration has been exceeded,
	 * advances the period index if so, and resets the period timer.
	 * Returns the new period timer value for the caller to store.
	 * 
	 * @param ActiveAbility  The currently active combat ability. Must be valid.
	 * @param SimPeriodTime  Accumulated time elapsed in the current period (seconds).
	 * @param MoveCompSpeed  MaxWalkSpeed from the character's movement component.
	 *                       Used only when the current period is EPeriodType::MoveTo.
	 * @param MoveCompAccel  MaxAcceleration from the movement component.
	 * @param CurrentSpeed   Current velocity magnitude of the simulated character.
	 * @param SimLocation    Current simulated world-space location of the character.
	 * @return               Updated SimPeriodTime. Caller must write this back to 
	 *                       UWolfPresageComponent::SimPeriodTime.
	 */
	static float AdvancePeriod(
		UBaseCombatAbility* ActiveAbility,
		float SimPeriodTime,
		float MoveCompSpeed,
		float MoveCompAccel,
		float CurrentSpeed,
		const FVector& SimLocation
	);
};
