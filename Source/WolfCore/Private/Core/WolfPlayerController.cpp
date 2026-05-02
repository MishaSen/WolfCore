// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Core/WolfPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Core/WolfFunctionLibrary.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "Systems/CombatModeSubsystem.h"
#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"
#include "WolfCore/Public/Input/WolfInputComponent.h"

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

	if (!EnhancedInputSubsystem) EnhancedInputSubsystem = GetEnhancedInputSubsystem();
	if (!WolfASC) WolfASC = GetASC();

	const FWolfGameplayTags& WolfTags = FWolfGameplayTags::Get();

	for (const auto InputStateTags = {WolfTags.InputState_RT, WolfTags.InputState_TB, WolfTags.InputState_OOC};
	     const auto& Tag : InputStateTags)
	{
		WolfASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::OnCombatTagChanged);
	}

	WolfASC->AddLooseGameplayTag(WolfTags.Event_ModeSwitchReady);
	WOLF_LOG(Log, TEXT("Player Controller ready to switch modes"));

	if (const auto* CombatModeSubsystem = GetWorld()->GetSubsystem<UCombatModeSubsystem>())
	{
		const auto CurrentModeTag = CombatModeSubsystem->GetCurrentMode();
		OnCombatTagChanged(CurrentModeTag, 1);
	}
}

void AWolfPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	auto* WolfInputComponent = CastChecked<UWolfInputComponent>(InputComponent);
	if (!WolfInputComponent) return;

	auto BindInputAction = [&](UInputAction* Action, auto Method)
	{
		if (Action) WolfInputComponent->BindAction(Action, ETriggerEvent::Triggered, this, Method);
		else WOLF_WARN(TEXT("Tried to bind an input action to a null action"));
	};

	BindInputAction(ScrubAction, &ThisClass::HandleScrubInput);
	BindInputAction(MoveAction, &ThisClass::Move);
	BindInputAction(LookAction, &ThisClass::Look);

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

void AWolfPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void AWolfPlayerController::OnCombatTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0) return;

	const auto TagName = Tag.ToString();

	if (const auto* IMC = CombatModeMappings.Find(Tag))
	{
		EnhancedInputSubsystem->ClearAllMappings();
		EnhancedInputSubsystem->AddMappingContext(*IMC, 0);
		WOLF_INFO(TEXT("Switched to %s mode"), *TagName);
	}
	else
	{
		WOLF_WARN(TEXT("No InputMapping set for CombatModeTag %s"), *TagName);
	}
}

void AWolfPlayerController::AbilityInputTagPressed(const FGameplayTag InputTag)
{
	WolfASC->AbilityInputTagPressed(InputTag);
	WOLF_LOG(Log, TEXT( "Ability [%s] activated." ), *InputTag.ToString());
}

void AWolfPlayerController::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	WolfASC->AbilityInputTagReleased(InputTag);
}

void AWolfPlayerController::AbilityInputTagHeld(const FGameplayTag InputTag)
{
	WolfASC->AbilityInputTagHeld(InputTag);
}

void AWolfPlayerController::HandleScrubInput(const FInputActionValue& Value)
{
	const auto AxisValue = Value.Get<float>();
	if (FMath::IsNearlyZero(AxisValue)) return;

	if (auto* CMS = UWolfFunctionLibrary::GetWorldSubsystem<UCombatModeSubsystem>(this))
	{
		if (!CMS->bIsInTB) return;

		const auto NewTime = CMS->GetCurrentTimelineTime() + AxisValue * 0.1f;
		CMS->ScrubTimeline(NewTime);
	}
}

void AWolfPlayerController::Move(const FInputActionValue& Value)
{
	const FRotator YawRotation(0, GetControlRotation().Yaw, 0);
	const FRotationMatrix RotationMatrix(YawRotation);

	const auto ForwardDirection = RotationMatrix.GetUnitAxis(EAxis::X);
	const auto RightDirection = RotationMatrix.GetUnitAxis(EAxis::Y);

	const auto MovementVector = Value.Get<FVector2D>();

	if (auto* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->AddMovementInput(ForwardDirection, MovementVector.Y);
		ControlledCharacter->AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AWolfPlayerController::Look(const FInputActionValue& Value)
{
	const auto LookVector = Value.Get<FVector2D>();
	if (auto* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->AddControllerYawInput(LookVector.X);
		ControlledCharacter->AddControllerPitchInput(LookVector.Y);
	}
}

UWolfAbilitySystemComponent* AWolfPlayerController::GetASC()
{
	if (WolfASC) return WolfASC;

	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		WolfASC = Cast<UWolfAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ControlledCharacter));
	}

	return WolfASC;
}

UEnhancedInputLocalPlayerSubsystem* AWolfPlayerController::GetEnhancedInputSubsystem()
{
	if (EnhancedInputSubsystem) return EnhancedInputSubsystem;

	EnhancedInputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());

	if (!EnhancedInputSubsystem)
	{
		WOLF_ERROR(TEXT("Failed to get EnhancedInputSubsystem from local player %s"), *GetLocalPlayer()->GetName());
	}

	return EnhancedInputSubsystem;
}
