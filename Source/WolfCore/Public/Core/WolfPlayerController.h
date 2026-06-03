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
	/** Default constructor for AWolfPlayerController, initializing input and ability system components. */
	AWolfPlayerController();

	/** Called every frame to update the player controller's tick behavior and handle per-frame logic. Overrides APlayerController::PlayerTick(). */
	/**
	 * @param DeltaTime The time elapsed since this tick was last executed in seconds.
	 */
	virtual void PlayerTick(float DeltaTime) override;

protected:
	/** Called when the player controller is first spawned and ready to begin functioning. Overrides APlayerController::BeginPlay(). */
	virtual void BeginPlay() override;

	/** Sets up the input component and binds all input actions to their corresponding handler functions. Overrides APlayerController::SetupInputComponent(). */
	virtual void SetupInputComponent() override;

	/** Called after initialization completes, finalizing component setup and binding input subsystems. Overrides AActor::PostInitializeComponents(). */
	virtual void PostInitializeComponents() override;

	// ============================================================================================================================
	// Input Behavior
	// ============================================================================================================================

private:
	/** Handles scrub input actions for temporal prediction animation preview and replay positioning. */
	/**
	 * @param Value The FInputActionValue containing the context and intensity of the scrub action.
	 */
	void HandleScrubInput(const FInputActionValue& Value);

	/** Handles movement input actions, translating player directional input into character movement commands. */
	/**
	 * @param Value The FInputActionValue containing direction and magnitude data for movement.
	 */
	void Move(const FInputActionValue& Value);

	/** Handles camera look/rotation input actions, processing mouse or controller rotation data. */
	/**
	 * @param Value The FInputActionValue containing rotation delta for camera orientation changes.
	 */
	void Look(const FInputActionValue& Value);

	/** Processes ability input tag press events, triggering corresponding abilities through the ASC. */
	/**
	 * @param InputTag The FGameplayTag representing the pressed ability input action.
	 */
	void AbilityInputTagPressed(const FGameplayTag InputTag);

	/** Processes ability input tag release events, releasing held abilities or triggering cancel actions. */
	/**
	 * @param InputTag The FGameplayTag representing the released ability input action.
	 */
	void AbilityInputTagReleased(const FGameplayTag InputTag);

	/** Handles continuous ability input hold events while the button is held down for sustained actions. */
	/**
	 * @param InputTag The FGameplayTag representing the held ability input action.
	 */
	void AbilityInputTagHeld(const FGameplayTag InputTag);

	// ============================================================================================================================
	// Combat Mode / Input Context Logic
	// ============================================================================================================================

	/** Handles changes to combat mode GameplayTags, updating input context and mapping accordingly. */
	/**
	 * @param Tag The FGameplayTag representing the new combat mode being entered or exited.
	 * @param NewCount The reference count of active tags indicating current combat state depth.
	 */
	void OnCombatTagChanged(const FGameplayTag Tag, int32 NewCount);

	// ============================================================================================================================
	// Accessors / Helpers
	// ============================================================================================================================

public:
	/** Retrieves the Ability System Component (ASC) for Gameplay Ability System interactions and ability management. */
	UWolfAbilitySystemComponent* GetASC();

	/**
	 * Applies the input mapping context for a given combat mode tag.
	 * Called by CombatModeSubsystem during mode transitions to ensure input context is always applied.
	 * @param Mode The FGameplayTag representing the combat mode to apply input mappings for.
	 */
	UFUNCTION(BlueprintCallable, Category = "WolfCore|Input")
	void ApplyInputMappingForMode(const FGameplayTag& Mode);
	
private:
	/** Retrieves the Enhanced Input Local Player subsystem for input system initialization and access. */
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem();

	// ============================================================================================================================
	// Member Variables - Input System Components
	// ============================================================================================================================

	/** Strong reference to the Enhanced Input Local Player subsystem for player-specific input handling. */
	UPROPERTY()
	TObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInputSubsystem;

	/** Input configuration asset that defines all input actions and bindings for this player controller. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Input")
	TObjectPtr<UWolfInputConfig> InputConfig;

	/** Map of combat mode GameplayTags to their corresponding input mapping context assets for dynamic binding switching. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Input")
	TMap<FGameplayTag, TObjectPtr<UInputMappingContext>> CombatModeMappings;

	// ============================================================================================================================
	// Member Variables - Input Actions
	// ============================================================================================================================

	/** Input action asset defining the scrub/preview action for temporal prediction animation control. */
	UPROPERTY(EditAnywhere, Category = "WolfCore|Input")
	TObjectPtr<UInputAction> ScrubAction;

	/** Input action asset defining the movement action for character directional input processing. */
	UPROPERTY(EditAnywhere, Category = "WolfCore|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Input action asset defining the look/rotation action for camera orientation control. */
	UPROPERTY(EditAnywhere, Category = "WolfCore|Input")
	TObjectPtr<UInputAction> LookAction;

	// ============================================================================================================================
	// Member Variables - Ability System
	// ============================================================================================================================

	/** Strong reference to the Ability System Component (ASC) providing Gameplay Ability System interactions for this controller. */
	UPROPERTY()
	TObjectPtr<UWolfAbilitySystemComponent> WolfASC;
};