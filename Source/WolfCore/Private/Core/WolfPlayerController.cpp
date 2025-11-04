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
#include "CombatMode.h"

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

	if (auto* ASC = GetASC())
	{
		const FWolfGameplayTags& WolfTags = FWolfGameplayTags::Get();

		ASC->RegisterGameplayTagEvent(WolfTags.InputState_RT, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::OnCombatTagChanged);
		ASC->RegisterGameplayTagEvent(WolfTags.InputState_TB, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::OnCombatTagChanged);
		ASC->RegisterGameplayTagEvent(WolfTags.InputState_OOC, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::OnCombatTagChanged);

		ASC->AddLooseGameplayTag(WolfTags.Event_ModeSwitchReady);
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

	CombatModeToInputContext.Add(ECombatMode::RT, EInputContext::InCombatRT);
	CombatModeToInputContext.Add(ECombatMode::TB, EInputContext::InCombatTB);
	CombatModeToInputContext.Add(ECombatMode::OOC, EInputContext::OutOfCombat);

	TagToCombatModePairs = {
		{ FWolfGameplayTags::Get().InputState_RT, ECombatMode::RT },
		{ FWolfGameplayTags::Get().InputState_TB, ECombatMode::TB },
		{ FWolfGameplayTags::Get().InputState_OOC, ECombatMode::OOC }
	};
}

void AWolfPlayerController::UpdateInputContext(const EInputContext NewInputContext)
{
	auto* EnhancedInputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!EnhancedInputSubsystem) return;

	EnhancedInputSubsystem->ClearAllMappings();

	if (const auto InputContext = InputContextMap.Find(NewInputContext); InputContext && *InputContext)
	{
		EnhancedInputSubsystem->AddMappingContext(*InputContext, 0);
	}
	
	CurrentInputContext = NewInputContext;
	WOLF_INFO(TEXT("Updated Input Context to %s"), *UEnum::GetValueAsString(CurrentInputContext));
}

void AWolfPlayerController::OnCombatTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0)
	{
		WOLF_LOG(Log, TEXT("Tag %s no longer on ASC"), *Tag.ToString());
		return;
	}

	for (const auto& [TagToCheck, Mode] : TagToCombatModePairs)
	{
		WOLF_LOG(Log, TEXT("TagToCheck is %s"), *TagToCheck.ToString());
		WOLF_LOG(Log, TEXT("Checking if ASC tag %s is tag %s"), *Tag.ToString(), *TagToCheck.ToString());
		if (Tag.MatchesTagExact(TagToCheck))
		{
			WOLF_LOG(Log, TEXT("Mode transition to %s"), *UEnum::GetValueAsString(Mode));
			HandleModeTransition(Mode);
			return;
		}
	}
}

void AWolfPlayerController::HandleModeTransition(ECombatMode NewMode)
{
	if (const auto NewContext = CombatModeToInputContext.Find(NewMode))
	{
		UpdateInputContext(*NewContext);
		WOLF_INFO(TEXT("Switched to %s mode"), *UEnum::GetValueAsString(NewMode));
	}
	else
	{
		WOLF_WARN(TEXT("No InputContext mapped for CombatMode %s"), *UEnum::GetValueAsString(NewMode));
	}
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
