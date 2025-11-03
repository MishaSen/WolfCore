// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Core/WolfPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/Character.h"
#include "WolfCore/Public/Input/WolfInputComponent.h"

AWolfPlayerController::AWolfPlayerController(): CurrentInputContext()
{
}

void AWolfPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
}

void AWolfPlayerController::BeginPlay()
{
	Super::BeginPlay();
	HandleModeTransition(RT);

	// NOTE: Only start in RT or TB for testing. The default start should be Out of Combat.

	if (APawn* ControlledPawn = GetPawn())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ControlledPawn))
		{
			const FWolfGameplayTags& Tag = FWolfGameplayTags::Get();
			ASC->RegisterGameplayTagEvent(Tag.InputState_TB, EGameplayTagEventType::NewOrRemoved).AddUObject(
				this, &ThisClass::OnPresageModeTagChanged);
		}
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

void AWolfPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	InputContextMap.Add(EInputContext::OutOfCombat, OutOfCombatContext);
	InputContextMap.Add(EInputContext::InCombatRT, InCombatRTContext);
	InputContextMap.Add(EInputContext::InCombatTB, InCombatTBContext);
}

void AWolfPlayerController::OnPresageModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		HandleModeTransition(TB);
	}
	else
	{
		HandleModeTransition(RT);
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

void AWolfPlayerController::HandleModeTransition(ECombatMode NewMode)
{
	const EInputContext NewInputContext = NewMode == RT ? EInputContext::InCombatRT : EInputContext::InCombatTB;
	UpdateInputContext(NewInputContext);
	WOLF_INFO(TEXT("Switched to %s mode"), NewMode == RT ? TEXT("RT") : TEXT("TB"));
}

void AWolfPlayerController::AbilityInputTagPressed(const FGameplayTag InputTag)
{
	if (GetASC())
	{
		GetASC()->AbilityInputTagPressed(InputTag);
		WOLF_INFO(TEXT("Ability Input Tag Pressed: %s"), *InputTag.ToString());
	}
}

void AWolfPlayerController::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (GetASC())
	{
		GetASC()->AbilityInputTagReleased(InputTag);
		WOLF_LOG(Log, TEXT("Ability Input Tag Released: %s"), *InputTag.ToString());
	}
}

void AWolfPlayerController::AbilityInputTagHeld(const FGameplayTag InputTag)
{
	if (GetASC())
	{
		GetASC()->AbilityInputTagHeld(InputTag);
		WOLF_LOG(Log, TEXT("Ability Input Tag Held: %s"), *InputTag.ToString());
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
