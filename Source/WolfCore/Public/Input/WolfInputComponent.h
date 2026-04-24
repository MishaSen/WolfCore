// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "WolfInputConfig.h"
#include "WolfInputComponent.generated.h"

// ============================================================================================================================
// Forward Declarations
// ============================================================================================================================

/**
 * Input component wrapper for binding ability actions with enhanced input support.
 */
UCLASS()
class WOLFCORE_API UWolfInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Action Binding
	// ============================================================================================================================

public:
	/** Binds ability actions from the Wolf input config to callback functions for pressed, released, and held events. Template helper for internal C++ use only. */
	/**
	 * @tparam UserClass The parent class type containing the bound member function pointers.
	 * @param InputConfig Pointer to the UWolfInputConfig containing input action to tag mappings.
	 * @param Object Pointer to the instance whose methods will be called for each trigger event.
	 * @param Pressed Function pointer to execute when an ability action is pressed (triggered).
	 * @param Released Function pointer to execute when an ability action is released (completed).
	 * @param Held Function pointer to execute when an ability action is held down (triggered continuously).
	 */
	template <class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
	void BindAbilityActions(const UWolfInputConfig* InputConfig, UserClass* Object, PressedFuncType Pressed,
	                        ReleasedFuncType Released, HeldFuncType Held)
	{
		check(InputConfig);

		for (const auto& [InputAction, InputTag] : InputConfig->InputActions)
		{
			if (!(InputAction && InputTag.IsValid())) continue;

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
};