// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Abilities/BaseCombatAbility.h"

float UBaseCombatAbility::GetPeriodDuration(const FCombatPeriod& Period) const
{
	if (Period.Montage) return Period.Montage->GetPlayLength();
	return Period.Duration;
}

float UBaseCombatAbility::CalculateProjectedImpactTime() const
{
	float TimeAccumulator = 0.f;
	for (const auto& Period : AbilitySequence)
	{
		if (Period.Type == EPeriodType::Attack)
		{
			if (Period.Montage)
			{
				for (const auto& NotifyEvent : Period.Montage->Notifies)
				{
					return TimeAccumulator + NotifyEvent.GetTriggerTime();
				}
			}
			return TimeAccumulator + Period.HitDelay;
		}
		TimeAccumulator += GetPeriodDuration(Period);
	}
	return -1.f;
}

bool UBaseCombatAbility::IsInvulnerableAt(float RelativeTime) const
{
	float TimeAccumulator = 0.f;
	for (const auto& Period : AbilitySequence)
	{
		float PeriodEnd = TimeAccumulator + GetPeriodDuration(Period);
		if (RelativeTime >= TimeAccumulator && RelativeTime < PeriodEnd)
		{
			return Period.Type == EPeriodType::Evasion;
		}
		TimeAccumulator = PeriodEnd;
	}
	return false;
}

void UBaseCombatAbility::HandleGameplayEventHit_Implementation(FGameplayEventData Payload)
{
}
