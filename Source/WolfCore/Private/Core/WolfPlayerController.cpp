// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Core/WolfPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "WolfCore/Public/AbilitySystem//WolfAbilitySystemComponent.h"
#include "Engine/LocalPlayer.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "WolfCore/Public/Input/WolfInputComponent.h"
#include "WolfCore/Public/Presage/PresageSubsystem.h"

AWolfPlayerController::AWolfPlayerController()
{
}

void AWolfPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
}

void AWolfPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UpdateInputContext(EInputContext::InCombatRT);

	if (UPresageSubsystem* PresageSubsystem = UPresageSubsystem::Get(GetWorld()))
	{
		PresageSubsystem->OnTransitionToTB.AddDynamic(this, &AWolfPlayerController::HandleTBTransition);
	}
}

void AWolfPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
		}
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
		}

		if (UWolfInputComponent* WolfInputComponent = Cast<UWolfInputComponent>(InputComponent))
		{
			WolfInputComponent->BindAbilityActions(
				InputConfig,
				this,
				&ThisClass::AbilityInputTagPressed,
				&ThisClass::AbilityInputTagReleased,
				&ThisClass::AbilityInputTagHeld
			);
		}
	}
}

void AWolfPlayerController::UpdateInputContext(const EInputContext NewInputContext)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
		GetLocalPlayer()))
	{
		Subsystem->ClearAllMappings();

		switch (NewInputContext)
		{
		case EInputContext::OutOfCombat:
			if (OutOfCombatContext)
			{
				Subsystem->AddMappingContext(OutOfCombatContext, 0);
			}
			break;
		case EInputContext::InCombatRT:
			if (InCombatRTContext)
			{
				Subsystem->AddMappingContext(InCombatRTContext, 0);
			}
			break;
		case EInputContext::InCombatTB:
			if (InCombatTBContext)
			{
				Subsystem->AddMappingContext(InCombatTBContext, 0);
			}
			break;

		default:
			break;
		}

		CurrentInputContext = NewInputContext;
	}
}

void AWolfPlayerController::AbilityInputTagPressed(const FGameplayTag InputTag)
{
	if (GetASC())
	{
		GetASC()->AbilityInputTagPressed(InputTag);
	}
}

void AWolfPlayerController::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (GetASC())
	{
		GetASC()->AbilityInputTagReleased(InputTag);
	}
}

void AWolfPlayerController::AbilityInputTagHeld(const FGameplayTag InputTag)
{
	if (GetASC())
	{
		GetASC()->AbilityInputTagHeld(InputTag);
	}
}

void AWolfPlayerController::Move(const FInputActionValue& Value)
{
	// Get 2D Vector value from Input Action
	const FVector2D MovementVector = Value.Get<FVector2D>();

	// Find out which way is forward
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	// Get forward and right vectors
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// Add movement
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->AddMovementInput(ForwardDirection, MovementVector.Y);
		ControlledCharacter->AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AWolfPlayerController::Look(const FInputActionValue& Value)
{
	// Get 2D vector value from Input Action
	const FVector2D LookVector = Value.Get<FVector2D>();

	// Add yaw and pitch input to the controller
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->AddControllerYawInput(LookVector.X);
		ControlledCharacter->AddControllerPitchInput(LookVector.Y);
	}
}

void AWolfPlayerController::HandleTBTransition(const bool bIsEnteringTB)
{
	const EInputContext NewInputContext = bIsEnteringTB ? EInputContext::InCombatTB : EInputContext::InCombatRT;
	UpdateInputContext(NewInputContext);
}

UWolfAbilitySystemComponent* AWolfPlayerController::GetASC()
{
	if (WolfASC == nullptr)
	{
		if (ACharacter* ControlledCharacter = GetCharacter())
		{
			WolfASC = Cast<UWolfAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ControlledCharacter));
		}
	}
	return WolfASC;
}
