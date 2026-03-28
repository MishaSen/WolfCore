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
#include "Interfaces/CombatModeListener.h"
#include "Presage/ActorSnapshot.h"
#include "Presage/Snapshot.h"

#include "WolfCharacterBase.generated.h"

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
	virtual void PossessedBy(AController* NewController) override;

	UFUNCTION(BlueprintPure, Category = "Wolf|Abilities")
	FGameplayAbilitySpecHandle GetAbilitySpecHandle(const TSubclassOf<UGameplayAbility>& AbilityClass) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wolf|Combat")
	void Die();

	UFUNCTION(BlueprintPure, Category = "Wolf|Presage")
	FTransform GetProjectedTransform(float FutureTimeDelta) const;

	UFUNCTION(BlueprintPure, Category = "Wolf|Presage")
	float GetTimeToNextHitImpact() const;

	void UpdateTemporalPreview(float PreviewTime);
	void ClearPredictionBuffer() { PredictionBuffer.Empty(); }
	const FActorSnapshot* GetSnapshotAtTime(float RelativeTime) const;

	void GetPresageCollisionDimensions(float& OutRadius, float& OutHalfHeight) const;
	bool IsInvulnerableAt(float RelativeTime) const;
	class UBaseCombatAbility* GetActiveCombatAbility() const;

	virtual void CreateSnapshot_Implementation(FActorSnapshot& NewSnapshot) override;
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& StoredSnapshot) override;

	void SimulateTick(float DeltaTime);

public:
	FActiveGameplayEffectHandle PresageEffectHandle;

protected:
	void SimulatePhysicsStep(float DeltaTime);
	void SimulateAnimationStep(float DeltaTime);
	static FTransform ExtractRootMotionAtTime(UAnimMontage* Montage, float Time);

	void SnapshotPhysics(FActorSnapshot& Snapshot) const;
	void SnapshotGAS(FActorSnapshot& Snapshot) const;
	void SnapshotAnim(FActorSnapshot& Snapshot) const;

	void RestorePhysics(const FActorSnapshot& Snapshot);
	void RestoreGAS(const FActorSnapshot& Snapshot);
	void RestoreAnim(const FActorSnapshot& Snapshot);

	UFUNCTION()
	void HandleCombatModeChanged(FGameplayTag NewMode);

	class UCombatModeSubsystem* GetCMS() const;
	FORCEINLINE UCharacterMovementComponent* GetMoveComp() const { return CachedMoveComp; }
	FORCEINLINE UAnimInstance* GetAnimInst() const { return CachedAnimInst; }
	UAnimMontage* GetWolfCurrentMontage() const { return CachedAnimInst ? CachedAnimInst->GetCurrentActiveMontage() : nullptr; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wolf|Abilities")
	TObjectPtr<UWolfAbilityComponent> AbilityControl;
	
	UPROPERTY(EditDefaultsOnly, Category = "Wolf|GAS|Setup")
	TSubclassOf<UWolfAbilitySystemComponent> AbilitySystemComponentClass;

	UPROPERTY(BlueprintReadOnly, Category = "Wolf|Cache")
	TWeakObjectPtr<UCombatModeSubsystem> CachedCMS;

	UPROPERTY(BlueprintReadOnly, Category = "Wolf|Cache")
	TObjectPtr<UCharacterMovementComponent> CachedMoveComp;

	UPROPERTY(BlueprintReadOnly, Category = "Wolf|Cache")
	TObjectPtr<UAnimInstance> CachedAnimInst;

	UPROPERTY(EditDefaultsOnly, Category = "Wolf|GAS|Abilities")
	TObjectPtr<UAbilityConfig> AbilityConfig;

	UPROPERTY()
	TArray<FActorSnapshot> PredictionBuffer;

private:
	bool bIsRestoringSnapshot = false;

	UPROPERTY()
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	UPROPERTY()
	TArray<FGameplayTag> GrantedAbilityTags;

	UPROPERTY(EditDefaultsOnly, Category = "Wolf|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;
}; 