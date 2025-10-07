// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Core/WolfPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"
#include "Engine/LocalPlayer.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "WolfCore/Public/Input/WolfInputComponent.h"
#include "WolfCore/Public/Presage/PresageSubsystem.h"

AWolfPlayerController::AWolfPlayerController(): CurrentInputContext()
{
	InputContextMap.Add(EInputContext::OutOfCombat, OutOfCombatContext);
	InputContextMap.Add(EInputContext::InCombatRT, InCombatRTContext);
	InputContextMap.Add(EInputContext::InCombatTB, InCombatTBContext);
}

void AWolfPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
}

void AWolfPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UpdateInputContext(EInputContext::InCombatRT);
	if (CurrentInputContext == EInputContext::InCombatRT)
	{
		GEngine->AddOnScreenDebugMessage(0, 3.f, FColor::Yellow, TEXT("CurrentInputContext is TB"));
	}

	if (UPresageSubsystem* PresageSubsystem = UPresageSubsystem::Get(GetWorld()))
	{
		PresageSubsystem->OnTransitionToTB.AddDynamic(this, &AWolfPlayerController::HandleTBTransition);
	}
}

void AWolfPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UWolfInputComponent* WolfInputComponent = CastChecked<UWolfInputComponent>(InputComponent))
	{
		if (MoveAction)
		{
			WolfInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
		}
		if (LookAction)
		{
			WolfInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
		}
		if (InputConfig)
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
	if (const auto Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->ClearAllMappings();

		if (const auto InputContext = InputContextMap.Find(NewInputContext))
		{
			if (*InputContext)
			{
				Subsystem->AddMappingContext(*InputContext, 0);
			}
		}
		CurrentInputContext = NewInputContext;
	}
}

void AWolfPlayerController::HandleTBTransition(const bool bIsEnteringTB)
{
	const EInputContext NewInputContext = bIsEnteringTB ? EInputContext::InCombatTB : EInputContext::InCombatRT;
	UpdateInputContext(NewInputContext);
}

void AWolfPlayerController::AbilityInputTagPressed(const FGameplayTag InputTag)
{
	if (GetASC())
	{
		GetASC()->AbilityInputTagPressed(InputTag);
		GEngine->AddOnScreenDebugMessage(1, 3.f, FColor::Red, *InputTag.ToString());
	}
}

void AWolfPlayerController::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (GetASC())
	{
		GetASC()->AbilityInputTagReleased(InputTag);
		GEngine->AddOnScreenDebugMessage(2, 3.f, FColor::Blue, *InputTag.ToString());
	}
}

void AWolfPlayerController::AbilityInputTagHeld(const FGameplayTag InputTag)
{
	if (GetASC())
	{
		GetASC()->AbilityInputTagHeld(InputTag);
		GEngine->AddOnScreenDebugMessage(3, 3.f, FColor::Green, *InputTag.ToString());
	}
}

void AWolfPlayerController::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->AddMovementInput(ForwardDirection, MovementVector.Y);
		ControlledCharacter->AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AWolfPlayerController::Look(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->AddControllerYawInput(LookVector.X);
		ControlledCharacter->AddControllerPitchInput(LookVector.Y);
	}
}

UWolfAbilitySystemComponent* AWolfPlayerController::GetASC()
{
	if (WolfASC == nullptr)
	{
		if (ACharacter* ControlledCharacter = GetCharacter())
		{
			WolfASC = Cast<UWolfAbilitySystemComponent>(
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ControlledCharacter));
		}
	}
	return WolfASC;
}
