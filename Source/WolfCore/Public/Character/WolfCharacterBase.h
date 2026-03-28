// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/WolfAbilitySystemComponent.h"
#include "Core/WolfAbilityComponent.h"
#include "Interfaces/CombatModeListener.h"
#include "Presage/ActorSnapshot.h"
#include "Presage/Snapshot.h"
#include "Systems/CombatModeSubsystem.h"

#include "WolfCharacterBase.generated.h"

class UWolfPresageComponent;
class UWolfAbilityComponent;
class UCharacterStatConfig;
class UAbilityConfig;
class UGameplayAbility;
class UWolfAbilitySystemComponent;
class UWolfAttributeSet;
/**
 * 
 */
class UGameplayEffect;

UCLASS(Blueprintable, BlueprintType)
class WOLFCORE_API AWolfCharacterBase : public ACharacter, public IAbilitySystemInterface, public ISnapshot, public ICombatModeListener
{
	GENERATED_BODY()

public:
	AWolfCharacterBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UCombatModeSubsystem* GetCMS() const;
	virtual void PossessedBy(AController* NewController) override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilityControl ? AbilityControl->GetWolfASC() : nullptr; }

	UFUNCTION(BlueprintPure, Category = "Wolf|Abilities")
	FGameplayAbilitySpecHandle GetAbilitySpecHandle(const TSubclassOf<UGameplayAbility>& AbilityClass) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wolf|Combat")
	void Die();

	UFUNCTION(BlueprintPure, Category = "Wolf|Presage")
	FTransform GetProjectedTransform(float FutureTimeDelta) const;

	UFUNCTION(BlueprintPure, Category = "Wolf|Presage")
	float GetTimeToNextHitImpact() const;

	void UpdateTemporalPreview(float PreviewTime);

	void GetPresageCollisionDimensions(float& OutRadius, float& OutHalfHeight) const;
	bool IsInvulnerableAt(float RelativeTime) const;
	UBaseCombatAbility* GetActiveCombatAbility() const;

	virtual void CreateSnapshot_Implementation(FActorSnapshot& NewSnapshot) override;
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot) override;

	void SimulateTick(float DeltaTime);
	void ClearPredictionBuffer();
	const FActorSnapshot* GetSnapshotAtTime(float RelativeTime) const;

public:
	FActiveGameplayEffectHandle PresageEffectHandle;

protected:
	static FTransform ExtractRootMotionAtTime(UAnimMontage* Montage, float Time);

	UFUNCTION()
	void HandleCombatModeChanged(FGameplayTag NewMode);
	
	FORCEINLINE UCharacterMovementComponent* GetMoveComp() const { return CachedMoveComp; }
	FORCEINLINE UAnimInstance* GetAnimInst() const { return CachedAnimInst; }
	UAnimMontage* GetWolfCurrentMontage() const { return CachedAnimInst ? CachedAnimInst->GetCurrentActiveMontage() : nullptr; }
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wolf|Abilities")
	TObjectPtr<UWolfAbilityComponent> AbilityControl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wolf|Abilities")
	TObjectPtr<UWolfPresageComponent> PresageControl;
	
	UPROPERTY(EditDefaultsOnly, Category = "Wolf|GAS|Abilities")
	TObjectPtr<UAbilityConfig> AbilityConfig;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> CachedMoveComp;

	UPROPERTY()
	TObjectPtr<UAnimInstance> CachedAnimInst;

	UPROPERTY()
	mutable TWeakObjectPtr<UCombatModeSubsystem> CachedCMS;

private:
	bool bIsRestoringSnapshot = false;

	UPROPERTY()
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	UPROPERTY()
	TArray<FGameplayTag> GrantedAbilityTags;

	UPROPERTY(EditDefaultsOnly, Category = "Wolf|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;
}; 