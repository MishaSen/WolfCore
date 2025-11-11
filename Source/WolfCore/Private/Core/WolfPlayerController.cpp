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
#include "Core/CombatModeData.h"

AWolfPlayerController::AWolfPlayerController(): CurrentInputContext(EInputContext::OutOfCombat)
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

	if (auto* WolfInputComponent = CastChecked<UWolfInputComponent>(InputComponent))
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

	if (!CombatModeDataTable)
	{
		WOLF_WARN(TEXT("No CombatModeDataTable set for %s"), *GetWorld()->GetName());
		return;
	}

	static const FString Context(TEXT("CombatModeTable"));
	TArray<FCombatModeInfo*> AllRows;
	CombatModeDataTable->GetAllRows(Context, AllRows);

	for (const auto* Row : AllRows)
	{
		if (!Row) continue;
		CombatModeToInputContext.Add(Row->Mode, Row->InputContext);
		InputContextMap.Add(Row->InputContext, Row->InputMapping);
		TagToCombatMode.Add(Row->Tag, Row->Mode);
	}
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
	if (NewCount <= 0) return;

	if (const auto* CombatMode = TagToCombatMode.Find(Tag))
	{
		HandleModeTransition(*CombatMode);
	}
	else
	{
		WOLF_WARN(TEXT("No CombatMode mapped for GameplayTag %s"), *Tag.ToString());
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
	WolfASC->AbilityInputTagPressed(InputTag);
	WOLF_INFO(TEXT("Ability Input Tag Pressed: %s"), *InputTag.ToString());
}

void AWolfPlayerController::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	WolfASC->AbilityInputTagReleased(InputTag);
	WOLF_LOG(Log, TEXT("Ability Input Tag Released: %s"), *InputTag.ToString());
}

void AWolfPlayerController::AbilityInputTagHeld(const FGameplayTag InputTag)
{
	WolfASC->AbilityInputTagHeld(InputTag);
	WOLF_LOG(Log, TEXT("Ability Input Tag Held: %s"), *InputTag.ToString());
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
