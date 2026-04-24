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
	/** Default constructor for AWolfChar, initializing the character's camera components. */
	AWolfChar();

	// ============================================================================================================================
	// Camera Components
	// ============================================================================================================================

	/** Spring arm component that serves as a boom between the character and the follow camera, providing distance-based offset. */
	UPROPERTY(VisibleAnywhere, Category = "WolfCore|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** First-person camera component attached to the spring arm for player viewport rendering. */
	UPROPERTY(VisibleAnywhere, Category = "WolfCore|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
};