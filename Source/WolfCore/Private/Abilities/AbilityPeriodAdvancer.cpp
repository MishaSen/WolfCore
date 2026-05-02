// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/AbilityPeriodAdvancer.h"
#include "Abilities/BaseCombatAbility.h"
#include "Debug/WolfDebug.h"

float FAbilityPeriodAdvancer::AdvancePeriod(
	UBaseCombatAbility* ActiveAbility,
	float SimPeriodTime,
	float MoveCompSpeed,
	float MoveCompAccel,
	float CurrentSpeed,
	const FVector& SimLocation)
{
	if (!IsValid(ActiveAbility))
	{
		return SimPeriodTime;
	}

	const TArray<FCombatPeriod>& AbilitySequence = ActiveAbility->GetAbilitySequence();
	const int32 CurrentIndex = ActiveAbility->GetCurrentPeriodIndex();

	if (!AbilitySequence.IsValidIndex(CurrentIndex))
	{
		return SimPeriodTime;
	}

	float Duration = 0.f;
	const FCombatPeriod& CurrentPeriod = AbilitySequence[CurrentIndex];

	if (CurrentPeriod.Type == EPeriodType::MoveTo)
	{
		const float DistanceToDestination = FVector::Distance(SimLocation, CurrentPeriod.MoveToDestination);
		Duration = ActiveAbility->CalculateMovementDuration(
			DistanceToDestination,
			MoveCompSpeed,
			MoveCompAccel,
			CurrentSpeed
		);
		WOLF_LOG(Log, TEXT("[CALCULATE MOVEMENT DURATION TEST] Duration is %f."), Duration);
	}
	else
	{
		Duration = ActiveAbility->GetPeriodDuration(CurrentPeriod);
	}

	if (SimPeriodTime >= Duration)
	{
		const int32 NextIndex = CurrentIndex + 1;
		ActiveAbility->SetCurrentPeriodIndex(NextIndex);
		
		if (AbilitySequence.IsValidIndex(NextIndex))
		{
			WOLF_LOG(Log, TEXT("[SIM] Period %d %s completed. Next: %d %s."),
				CurrentIndex, *UEnum::GetValueAsString(AbilitySequence[CurrentIndex].Type),
				NextIndex, *UEnum::GetValueAsString(AbilitySequence[NextIndex].Type));
		}
		else
		{
			WOLF_LOG(Log, TEXT("[SIM] Ability Sequence finished at Step %d."), CurrentIndex);
		}

		return 0.f; // Reset SimPeriodTime
	}

	return SimPeriodTime;
}
