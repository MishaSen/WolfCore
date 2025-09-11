// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "WolfInputConfig.h"

#include "WolfInputComponent.generated.h"

struct FTaggedInputAction;
class UWolfInputConfig;
/**
 * 
 */
UCLASS()
class WOLFCORE_API UWolfInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	template <class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
	void BindAbilityActions(const UWolfInputConfig* InputConfig, UserClass* Object, PressedFuncType Pressed,
	                        ReleasedFuncType Released, HeldFuncType Held)
	{
		check(InputConfig);

		for (const auto& [InputAction, InputTag] : InputConfig->InputActions)
		{
			if (InputAction && InputTag.IsValid())
			{
				if (Pressed)
				{
					BindAction(InputAction, ETriggerEvent::Started, Object, Pressed, InputTag);
				}
				if (Released)
				{
					BindAction(InputAction, ETriggerEvent::Completed, Object, Released, InputTag);
				}
				if (Held)
				{
					BindAction(InputAction, ETriggerEvent::Triggered, Object, Held, InputTag);
				}
			}
		}
	}
};
