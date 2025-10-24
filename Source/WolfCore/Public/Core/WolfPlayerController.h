// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/WolfAbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/HitResult.h"

#include "WolfPlayerController.generated.h"

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
UENUM()
enum ECombatMode
{
	RT, TB
};

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
	void HandleModeTransition(ECombatMode NewMode = RT);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PostInitializeComponents() override;

	UFUNCTION()
	void OnPresageModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

private:
	// --- Input ---
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UWolfInputConfig> InputConfig;

	TMap<EInputContext, TObjectPtr<UInputMappingContext>> InputContextMap;
	
	UPROPERTY(EditAnywhere, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> OutOfCombatContext;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InCombatRTContext;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InCombatTBContext;

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
	UWolfAbilitySystemComponent* GetASC();
};
