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

	EnhancedInputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());

	if (auto* ASC = GetASC())
	{
		const FWolfGameplayTags& WolfTags = FWolfGameplayTags::Get();

		for (const auto InputStateTags = {WolfTags.InputState_RT, WolfTags.InputState_TB, WolfTags.InputState_OOC};
		     const auto& Tag : InputStateTags)
		{
			ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddUObject(
				this, &ThisClass::OnCombatTagChanged);
		}

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
	TArray<FCombatModeInfo*> Rows;
	CombatModeDataTable->GetAllRows(Context, Rows);

	if (Rows.IsEmpty())
	{
		WOLF_WARN(TEXT("CombatModeTable empty"));
		return;
	}

	CachedCombatModeRows = Rows;
	WOLF_LOG(Log, TEXT("CombatModeTable loaded with %d rows"), CachedCombatModeRows.Num());
}

void AWolfPlayerController::UpdateInputContext(const EInputContext NewInputContext)
{
	if (!EnhancedInputSubsystem) return;

	EnhancedInputSubsystem->ClearAllMappings();

	if (const auto* Info = FindCombatModeInfo(NewInputContext))
	{
		EnhancedInputSubsystem->AddMappingContext(Info->InputMapping, 0);
	}

	CurrentInputContext = NewInputContext;
	WOLF_INFO(TEXT("Updated Input Context to %s"), *UEnum::GetValueAsString(CurrentInputContext));
}

void AWolfPlayerController::OnCombatTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0) return;

	if (const auto* Info = FindCombatModeInfo(Tag))
	{
		HandleModeTransition(Info->Mode);
	}
	else
	{
		WOLF_WARN(TEXT("No CombatMode mapped for GameplayTag %s"), *Tag.ToString());
	}
}

void AWolfPlayerController::HandleModeTransition(ECombatMode NewMode)
{
	if (const auto* Info = FindCombatModeInfo(NewMode))
	{
		UpdateInputContext(Info->InputContext);
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

template<typename T>
const FCombatModeInfo* AWolfPlayerController::FindCombatModeInfo(const T& MatchValue) const
{
	if (!CombatModeDataTable) return nullptr;

	static const FString Context (TEXT("CombatModeLookup"));
	TArray<FCombatModeInfo*> Rows;
	CombatModeDataTable->GetAllRows(Context, Rows);

	for (const auto* Row : Rows)
	{
		if (!Row) continue;

		if constexpr (std::is_same_v<T, FGameplayTag>)
		{
			if (Row->Tag == MatchValue) return Row;
		}
		else if constexpr (std::is_same_v<T, ECombatMode>)
		{
			if (Row->Mode == MatchValue) return Row;
		}
		else if constexpr (std::is_same_v<T, EInputContext>)
		{
			if (Row->InputContext == MatchValue) return Row;
		}
		else if constexpr (std::is_same_v<T, TObjectPtr<UInputMappingContext>>)
		{
			if (Row->InputMapping == MatchValue) return Row;
		}
	}
	
	return nullptr;
}