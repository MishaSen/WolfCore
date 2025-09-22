// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/AbilityConfig.h"

void UAbilityConfig::GetAbilitiesByTag(const FGameplayTag& SearchTag, TArray<FAbilityInfo>& OutAbilities) const
{
	OutAbilities.Empty();
	for (const FAbilityInfo& AbilityInfo : CharacterAbilities)
	{
		if (AbilityInfo.AbilityTag.MatchesTag(SearchTag) || AbilityInfo.AdditionalAbTags.HasTag(SearchTag))
		{
			OutAbilities.Add(AbilityInfo);
		}
	}
}