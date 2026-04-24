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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wolf|Components")
	TObjectPtr<UWolfAbilityComponent> AbilityControl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wolf|Components")
	TObjectPtr<UWolfPresageComponent> PresageControl;

	// ============================================================================================================================
	// Configuration Data
	// ============================================================================================================================

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Wolf|Data")
	TObjectPtr<UAbilityConfig> AbilityConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Wolf|Data")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	AWolfCharacterBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;

	// ============================================================================================================================
	// Public API - Ability System & Combat Mode
	// ============================================================================================================================

public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilityControl ? AbilityControl->GetWolfASC() : nullptr; }
	UCombatModeSubsystem* GetCMS() const;

	UFUNCTION(BlueprintCallable, Category = "Wolf|Presage")
	UWolfPresageComponent* GetPresageComponent() const { return PresageControl; }

	UBaseCombatAbility* GetActiveCombatAbility() const;

	// ============================================================================================================================
	// Public API - Combat Queries
	// ============================================================================================================================

	UFUNCTION(BlueprintPure, Category = "Wolf|Combat")
	bool IsInvulnerableAt(float RelativeTime) const;

	UFUNCTION(BlueprintPure, Category = "Wolf|Combat")
	float GetTimeToNextHitImpact() const;

	UFUNCTION(BlueprintPure, Category = "Wolf|Presage")
	FTransform GetProjectedTransform(float FutureTimeDelta) const;

	// ============================================================================================================================
	// Public API - Abilities
	// ============================================================================================================================

	UFUNCTION(BlueprintPure, Category = "Wolf|Abilities")
	FGameplayAbilitySpecHandle GetAbilitySpecHandle(const TSubclassOf<UGameplayAbility>& AbilityClass) const;

	// ============================================================================================================================
	// Public API - Snapshot & Temporal
	// ============================================================================================================================

	void UpdateTemporalPreview(float PreviewTime);
	void GetPresageCollisionDimensions(float& OutRadius, float& OutHalfHeight) const;

	virtual void CreateSnapshot_Implementation(FActorSnapshot& NewSnapshot) override;
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot) override;

	// ============================================================================================================================
	// Public API - Death
	// ============================================================================================================================

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wolf|Combat")
	void Die();

	// ============================================================================================================================
	// Utilities
	// ============================================================================================================================

	FORCEINLINE UAnimInstance* GetAnimInst() const { return CachedAnimInst; }

	// ============================================================================================================================
	// State
	// ============================================================================================================================

public:
	FActiveGameplayEffectHandle PresageEffectHandle;

	bool bIsRestoringSnapshot = false;

	// ============================================================================================================================
	// Protected - Combat Mode Handling
	// ============================================================================================================================

protected:
	UFUNCTION()
	void HandleCombatModeChanged(FGameplayTag NewMode);

	static FTransform ExtractRootMotionAtTime(UAnimMontage* Montage, float Time);
	UAnimMontage* GetWolfCurrentMontage() const { return CachedAnimInst ? CachedAnimInst->GetCurrentActiveMontage() : nullptr; }

	// ============================================================================================================================
	// Cached References & Internal State
	// ============================================================================================================================

protected:
	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> CachedAnimInst;

	UPROPERTY(Transient)
	mutable TWeakObjectPtr<UCombatModeSubsystem> CachedCMS;

private:
	UPROPERTY(Transient)
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	UPROPERTY(Transient)
	TArray<FGameplayTag> GrantedAbilityTags;
};