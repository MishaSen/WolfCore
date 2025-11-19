// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CombatMode.h"
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
UENUM(BlueprintType)
enum class EInputContext : uint8
{
	OutOfCombat,
	InCombatRT,
	InCombatTB
};

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
	void UpdateInputContext(const EInputContext NewInputContext);
	void OnCombatTagChanged(const FGameplayTag Tag, int32 NewCount);
	
	UFUNCTION()
	void ApplyCombatMode(ECombatMode NewMode);

	template <class T>
	const FCombatModeInfo* FindCombatModeInfo(const T& MatchValue) const;

#pragma endregion

#pragma region Accessors / Helpers

public:
	UWolfAbilitySystemComponent* GetASC();

#pragma endregion
	
#pragma region Variables
	
	// Input System Components
	
private:
	UPROPERTY()
	TObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInputSubsystem;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UWolfInputConfig> InputConfig;

	// Input Actions

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;
	
	// Combat Mode Data / Mapping

public:
	EInputContext CurrentInputContext;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Combat Mode")
	TObjectPtr<UDataTable> CombatModeDataTable;
	
	TArray<FCombatModeInfo*> CachedCombatModeRows;

	// Ability System
	
	UPROPERTY()
	TObjectPtr<UWolfAbilitySystemComponent> WolfASC;

#pragma endregion
};