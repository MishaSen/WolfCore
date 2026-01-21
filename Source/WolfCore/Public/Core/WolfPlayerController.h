// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/WolfAbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/HitResult.h"

#include "WolfPlayerController.generated.h"

struct FCombatModeInfo;
class UEnhancedInputLocalPlayerSubsystem;
struct FGameplayEventData;
struct FInputActionValue;
struct FGameplayTag;
class UWolfAbilitySystemComponent;
class UWolfInputConfig;
class UInputMappingContext;
class UInputAction;
/**
 * 
 */

UCLASS()
class WOLFCORE_API AWolfPlayerController : public APlayerController
{
	GENERATED_BODY()
	

#pragma region Lifecycle Hooks
	
public:
	AWolfPlayerController();
	virtual void PlayerTick(float DeltaTime) override;
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PostInitializeComponents() override;
	
#pragma endregion
	

#pragma region Input Behavior
	
private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void AbilityInputTagPressed(const FGameplayTag InputTag);
	void AbilityInputTagReleased(const FGameplayTag InputTag);
	void AbilityInputTagHeld(const FGameplayTag InputTag);

#pragma endregion

#pragma region Combat Mode / Input Context Logic

private:
	void OnCombatTagChanged(const FGameplayTag Tag, int32 NewCount);

#pragma endregion

#pragma region Accessors / Helpers

public:
	UWolfAbilitySystemComponent* GetASC();

private:
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem();

#pragma endregion
	
#pragma region Variables
	
	// Input System Components
	
private:
	UPROPERTY()
	TObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInputSubsystem;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UWolfInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TMap<FGameplayTag, TObjectPtr<UInputMappingContext>> CombatModeMappings;

	// Input Actions

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;
	
	// Ability System
	
	UPROPERTY()
	TObjectPtr<UWolfAbilitySystemComponent> WolfASC;

#pragma endregion
};