// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "Interfaces/CombatModeListener.h"
#include "GameplayAbilitySpecHandle.h"
#include "Core/WolfAbilityComponent.h"
#include "Presage/ActorSnapshot.h"
#include "Presage/Snapshot.h"
#include "WolfCharacterBase.generated.h"

class UAbilityConfig;
class UAnimInstance;
class UBaseCombatAbility;
class UCombatModeSubsystem;
class UGameplayAbility;
class UWolfPresageComponent;

/**
 * Base character class implementing the Ability System Interface, Snapshot interface, and Combat Mode Listener.
 * Serves as the foundation for all playable characters in the Wolf framework.
 */
UCLASS(Blueprintable, BlueprintType)
class WOLFCORE_API AWolfCharacterBase : public ACharacter, public IAbilitySystemInterface, public ISnapshot, public ICombatModeListener
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Components
	// ============================================================================================================================

protected:
	/** Component that manages gameplay abilities for this character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WolfCore|Components")
	TObjectPtr<UWolfAbilityComponent> AbilityControl;

	/** Component responsible for presage simulation and temporal prediction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WolfCore|Components")
	TObjectPtr<UWolfPresageComponent> PresageControl;

	// ============================================================================================================================
	// Configuration Data
	// ============================================================================================================================

protected:
	/** Configuration asset that defines ability parameters and behavior settings. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Data")
	TObjectPtr<UAbilityConfig> AbilityConfig;

	/** List of gameplay abilities to grant to the character on spawn. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Data")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	/** Default constructor for AWolfCharacterBase. */
	AWolfCharacterBase();

	/** Called at runtime when the actor is ready to begin functioning. Overrides AActor::BeginPlay(). */
	virtual void BeginPlay() override;

	/** Called before the actor is destroyed. Overrides AActor::EndPlay(). */
	/**
	 * @param EndPlayReason The reason why the play is ending (e.g., Destroyed, SwitchLevel).
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Called when this character is possessed by a controller. Overrides ACharacter::PossessedBy(). */
	/**
	 * @param NewController The controller that has taken possession of this character.
	 */
	virtual void PossessedBy(AController* NewController) override;

	// ============================================================================================================================
	// Public API - Ability System & Combat Mode
	// ============================================================================================================================

public:
	/** Retrieves the Ability System Component (ASC) used for Gameplay Ability System interactions. Implements IAbilitySystemInterface. */
	/**
	 * @return Pointer to the AbilitySystemComponent, or nullptr if not initialized.
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilityControl ? AbilityControl->GetWolfASC() : nullptr; }

	/** Retrieves the Combat Mode subsystem singleton for this character. */
	/**
	 * @return Pointer to the UCombatModeSubsystem, or nullptr if unavailable.
	 */
	UCombatModeSubsystem* GetCMS() const;

	/** Returns the presage control component for temporal prediction queries. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Get Presage Component"), Category = "WolfCore|Presage")
	/**
	 * @return Pointer to the UWolfPresageComponent.
	 */
	UWolfPresageComponent* GetPresageComponent() const { return PresageControl; }

	/** Retrieves the currently active combat ability instance, if any. */
	/**
	 * @return Pointer to the active UBaseCombatAbility, or nullptr if none is active.
	 */
	UBaseCombatAbility* GetActiveCombatAbility() const;

	// ============================================================================================================================
	// Public API - Combat Queries
	// ============================================================================================================================

	/** Checks whether the character is invulnerable at a given time relative to the current prediction window. */
	UFUNCTION(BlueprintPure, Meta = (DisplayName = "Is Invulnerable"), Category = "WolfCore|Combat")
	/**
	 * @param RelativeTime Time delta to query against the prediction buffer.
	 * @return True if the character is invulnerable at the specified time; false otherwise.
	 */
	bool IsInvulnerableAt(float RelativeTime) const;

	/** Returns the time until the next hit impact based on the current animation and prediction state. */
	UFUNCTION(BlueprintPure, Meta = (DisplayName = "Get Time To Next Hit Impact"), Category = "WolfCore|Combat")
	/**
	 * @return Float representing the time remaining until the next hit impact in seconds.
	 */
	float GetTimeToNextHitImpact() const;

	/** Computes the projected transform at a future time delta for temporal prediction. */
	UFUNCTION(BlueprintPure, Meta = (DisplayName = "Get Projected Transform"), Category = "WolfCore|Presage")
	/**
	 * @param FutureTimeDelta Time delta into the future to project the transform.
	 * @return FTransform representing the projected world-space transform at the future time.
	 */
	FTransform GetProjectedTransform(float FutureTimeDelta) const;

	// ============================================================================================================================
	// Public API - Abilities
	// ============================================================================================================================

	/** Retrieves the Gameplay Ability Spec handle for a given ability class, granting Blueprint access to ability IDs. */
	UFUNCTION(BlueprintPure, Meta = (DisplayName = "Get Ability Spec Handle"), Category = "WolfCore|Abilities")
	/**
	 * @param AbilityClass The subclass of UGameplayAbility to query for its spec handle.
	 * @return FGameplayAbilityHandle representing the granted ability specification handle.
	 */
	FGameplayAbilitySpecHandle GetAbilitySpecHandle(const TSubclassOf<UGameplayAbility>& AbilityClass) const;

	// ============================================================================================================================
	// Public API - Snapshot & Temporal
	// ============================================================================================================================

	/** Updates the temporal preview state for prediction buffer visualization and debugging. */
	void UpdateTemporalPreview(float PreviewTime);

	/** Retrieves the collision bounding dimensions (radius and half-height) of the character's projected shape. */
	/**
	 * @param OutRadius Output parameter for the predicted collision radius in centimeters.
	 * @param OutHalfHeight Output parameter for the predicted collision half-height in centimeters.
	 */
	void GetPresageCollisionDimensions(float& OutRadius, float& OutHalfHeight) const;

	/** Creates a full actor snapshot representing the current state for temporal prediction storage. Implements ISnapshot. */
	/**
	 * @param NewSnapshot Reference to the FActorSnapshot to populate with current state data.
	 */
	virtual void CreateSnapshot_Implementation(FActorSnapshot& NewSnapshot) override;

	/** Restores actor state from a previously captured snapshot, reversing temporal prediction changes. Implements ISnapshot. */
	/**
	 * @param Snapshot The FActorSnapshot containing the state to restore.
	 */
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot) override;

	// ============================================================================================================================
	// Public API - Death
	// ============================================================================================================================

	/** Handles character death logic, including cleanup and notification dispatch. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Meta = (DisplayName = "Die"), Category = "WolfCore|Combat")
	void Die();

	// ============================================================================================================================
	// Utilities
	// ============================================================================================================================

	/** Provides fast access to the cached animation instance for runtime queries. */
	FORCEINLINE UAnimInstance* GetAnimInst() const { return CachedAnimInst; }

	// ============================================================================================================================
	// State
	// ============================================================================================================================

public:
	/** Handle to the active Gameplay Effect used for presage simulation state management. */
	FActiveGameplayEffectHandle PresageEffectHandle;

	/** Flag indicating whether the character is currently in a snapshot restoration sequence. */
	bool bIsRestoringSnapshot = false;

	// ============================================================================================================================
	// Protected - Combat Mode Handling
	// ============================================================================================================================

protected:
	/** Handles changes to the combat mode tag, updating ability state and animation layers accordingly. */
	/**
	 * @param NewMode The GameplayTag representing the new combat mode being entered.
	 */
	UFUNCTION()
	void HandleCombatModeChanged(FGameplayTag NewMode);

	/** Extracts root motion transform from an animation montage at a specific time position. Static helper for temporal scrubbing. */
	/**
	 * @param Montage The UAnimMontage to extract root motion data from.
	 * @param Time The playback time in seconds within the montage.
	 * @return FTransform representing the root motion transform at the specified time.
	 */
	static FTransform ExtractRootMotionAtTime(UAnimMontage* Montage, float Time);

	/** Retrieves the current active animation montage from the cached anim instance. */
	UAnimMontage* GetWolfCurrentMontage() const { return CachedAnimInst ? CachedAnimInst->GetCurrentActiveMontage() : nullptr; }

	// ============================================================================================================================
	// Cached References & Internal State
	// ============================================================================================================================

protected:
	/** Cached reference to the character's animation instance for performance optimization. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> CachedAnimInst;

	/** Weak reference cache for the Combat Mode subsystem to avoid repeated lookups. */
	UPROPERTY(Transient)
	mutable TWeakObjectPtr<UCombatModeSubsystem> CachedCMS;

private:
	/** Map tracking granted ability handles by their class for lifecycle management. */
	UPROPERTY(Transient)
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	/** Array of gameplay tags representing abilities that have been granted to this character. */
	UPROPERTY(Transient)
	TArray<FGameplayTag> GrantedAbilityTags;
};