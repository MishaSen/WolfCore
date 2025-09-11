// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Input/WolfInputConfig.h"

#include "InputAction.h"

const UInputAction* UWolfInputConfig::FindAbilityInputActionForTag(const FGameplayTag& Tag,
                                                                   const bool bLogNotFound) const
{
	for (const auto& [InputAction, InputTag] : InputActions)
	{
		if (InputAction && InputTag.MatchesTagExact(Tag))
		{
			return InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT(
			       "Can't find AbilityInputAction for InputTag [%s], on InputConfig [%s]"),
		       *Tag.ToString(),
		       *GetNameSafe(this));
	}

	return nullptr;
}
