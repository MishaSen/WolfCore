// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/WolfCharacterBase.h"
#include "WolfChar.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * Default player character with camera boom and follow camera setup.
 */
UCLASS(Blueprintable)
class WOLFCORE_API AWolfChar : public AWolfCharacterBase
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	AWolfChar();

	// ============================================================================================================================
	// Camera Components
	// ============================================================================================================================

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
};