// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/HitResult.h"

#include "WolfPlayerController.generated.h"

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
	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	// --- Input ---
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UWolfInputConfig> InputConfig;
	
	UPROPERTY(EditAnywhere, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> OutOfCombatContext;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InCombatRTContext;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InCombatTBContext;
	
	EInputContext CurrentInputContext = EInputContext::OutOfCombat;

	void UpdateInputContext(const EInputContext NewInputContext);
	void AbilityInputTagPressed(const FGameplayTag InputTag);
	void AbilityInputTagReleased(const FGameplayTag InputTag);
	void AbilityInputTagHeld(const FGameplayTag InputTag);

	UFUNCTION()
	void HandleTBTransition(bool bIsEnteringTB);

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
