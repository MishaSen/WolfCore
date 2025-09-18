// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/WolfCharacterBase.h"
#include "WolfCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UWolfInputConfig;

UCLASS()
class WOLFCORE_API AWolfCharacter : public AWolfCharacterBase
{
	GENERATED_BODY()

public:
	AWolfCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// --- Camera ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

protected:
	virtual void BeginPlay() override;
};
