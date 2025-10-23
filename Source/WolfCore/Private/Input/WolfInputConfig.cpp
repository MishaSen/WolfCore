// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Input/WolfInputConfig.h"

#include "InputAction.h"
#include "Debug/WolfDebug.h"

const UInputAction* UWolfInputConfig::FindAbilityInputActionForTag(const FGameplayTag& Tag,
                                                                   const bool bLogNotFound) const
{
	for (const auto& [InputAction, InputTag] : InputActions)
	{
		if (InputAction && InputTag.MatchesTagExact(Tag))
		{
			WOLF_LOG(Log, TEXT("Found AbilityInputAction for InputTag [%s]"), *Tag.ToString());
			return InputAction;
		}
	}

	if (bLogNotFound)
	{
		WOLF_WARN(TEXT("No AbilityInputAction found for InputTag [%s]"), *Tag.ToString());
	}

	return nullptr;
}
