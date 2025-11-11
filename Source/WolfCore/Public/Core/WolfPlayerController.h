// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CombatMode.h"
#include "AbilitySystem/WolfAbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/HitResult.h"
#include "Interaction/CombatModeListener.h"

#include "WolfPlayerController.generated.h"

enum class ECombatMode : uint8;
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

UENUM(BlueprintType)
enum class ETargetingStatus : uint8
{
	TargetingEnemy,
	TargetingNonEnemy,
	None
};

UCLASS()
class WOLFCORE_API AWolfPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWolfPlayerController();
	EInputContext CurrentInputContext;
	
	virtual void PlayerTick(float DeltaTime) override;
	
	UFUNCTION()
	void HandleModeTransition(ECombatMode NewMode);

	UWolfAbilitySystemComponent* GetASC();
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PostInitializeComponents() override;

private:
	// --- Input ---
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UWolfInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Mode")
	TObjectPtr<UDataTable> CombatModeDataTable;

	TMap<EInputContext, TObjectPtr<UInputMappingContext>> InputContextMap;
	TMap<ECombatMode, EInputContext> CombatModeToInputContext;
	TMap<FGameplayTag, ECombatMode> TagToCombatMode;
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;
	
	void AbilityInputTagPressed(const FGameplayTag InputTag);
	void AbilityInputTagReleased(const FGameplayTag InputTag);
	void AbilityInputTagHeld(const FGameplayTag InputTag);

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	void UpdateInputContext(const EInputContext NewInputContext);

	void OnCombatTagChanged(const FGameplayTag Tag, int32 NewCount);

	/* --- Cursor Trace ---
	TObjectPtr<AActor> LastActor;
	TObjectPtr<AActor> ThisActor;
	FHitResult CursorTraceHit;
	void CursorTrace();
	static void HighlightActor(AActor* InActor);
	static void UnhighlightActor(AActor* InActor);*/

	// --- Ability System ---
	UPROPERTY()
	TObjectPtr<UWolfAbilitySystemComponent> WolfASC;
};
