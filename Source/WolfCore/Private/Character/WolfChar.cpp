// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/WolfChar.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"


AWolfChar::AWolfChar()
{
	PrimaryActorTick.bCanEverTick = true;

	// --- Create Camera Components ---
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Boom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 500.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Follow Camera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false; // Camera does not need to rotate relative to boom
}

void AWolfChar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AWolfChar::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AWolfChar::BeginPlay()
{
	Super::BeginPlay();
}

