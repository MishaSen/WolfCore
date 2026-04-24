// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "WolfPlayerController.generated.h"

// ============================================================================================================================
// Forward Declarations
// ============================================================================================================================

struct FGameplayEventData;
struct FInputActionValue;

class UEnhancedInputLocalPlayerSubsystem;
class UWolfAbilitySystemComponent;
class UWolfInputConfig;
class UInputMappingContext;
class UInputAction;

/**
 * Player Controller managing input, combat mode context switching, and ability system integration.
 */
UCLASS()
class WOLFCORE_API AWolfPlayerController : public APlayerController
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle Hooks
	// ============================================================================================================================

public:
	AWolfPlayerController();
	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PostInitializeComponents() override;

	// ============================================================================================================================
	// Input Behavior
	// ============================================================================================================================

private:
	void HandleScrubInput(const FInputActionValue& Value);
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void AbilityInputTagPressed(const FGameplayTag InputTag);
	void AbilityInputTagReleased(const FGameplayTag InputTag);
	void AbilityInputTagHeld(const FGameplayTag InputTag);

	// ============================================================================================================================
	// Combat Mode / Input Context Logic
	// ============================================================================================================================

	void OnCombatTagChanged(const FGameplayTag Tag, int32 NewCount);

	// ============================================================================================================================
	// Accessors / Helpers
	// ============================================================================================================================

public:
	UWolfAbilitySystemComponent* GetASC();

private:
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem();

	// ============================================================================================================================
	// Member Variables - Input System Components
	// ============================================================================================================================

	UPROPERTY()
	TObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInputSubsystem;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UWolfInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TMap<FGameplayTag, TObjectPtr<UInputMappingContext>> CombatModeMappings;

	// ============================================================================================================================
	// Member Variables - Input Actions
	// ============================================================================================================================

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ScrubAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// ============================================================================================================================
	// Member Variables - Ability System
	// ============================================================================================================================

	UPROPERTY()
	TObjectPtr<UWolfAbilitySystemComponent> WolfASC;
};