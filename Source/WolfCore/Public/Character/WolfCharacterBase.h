// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Interfaces/CombatModeListener.h"
#include "Presage/Snapshot.h"

#include "WolfCharacterBase.generated.h"

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

#pragma region Presage System
	
public:
	UFUNCTION(BlueprintPure, Category = "Wolf|Presage")
	FTransform GetProjectedTransform(float FutureTimeDelta) const;

	UFUNCTION(BlueprintPure, Category = "Wolf|Presage")
	float GetTimeToNextHitImpact() const;

	virtual void CreateSnapshot_Implementation(FActorSnapshot& OutSnapshot) override;

	// NOTE: May need boolean to silence delegates that trigger animations and sounds based on attribute changes
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& InSnapshot) override;

	void GetPresageCollisionDimensions(float& OutRadius, float& OutHalfHeight) const;
	bool IsInvulnerableAt(float RelativeTime) const;
	class UBaseCombatAbility* GetActiveCombatAbility() const;
	
protected:
	static FTransform ExtractRootMotionAtTime(UAnimMontage* Montage, float Time);

	// --- Snapshot Helpers ---
	void SnapshotPhysics(FActorSnapshot& Snapshot) const;
	void SnapshotGAS(FActorSnapshot& Snapshot) const;
	void SnapshotAnim(FActorSnapshot& Snapshot) const;

	void RestorePhysics(const FActorSnapshot& Snapshot);
	void RestoreGAS(const FActorSnapshot& Snapshot);
	void RestoreAnim(const FActorSnapshot& Snapshot);

private:
	bool bIsRestoringSnapshot = false;
	
#pragma endregion

public:
	AWolfCharacterBase();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Abilities")
	FGameplayAbilitySpecHandle GetAbilitySpecHandle(const TSubclassOf<UGameplayAbility>& AbilityClass) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	void Die();

	FActiveGameplayEffectHandle PresageEffectHandle;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "GAS | Ability System")
	TSubclassOf<UWolfAbilitySystemComponent> AbilitySystemComponentClass;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS | Ability System")
	TObjectPtr<UWolfAbilitySystemComponent> ASC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS | Attributes")
	TObjectPtr<UWolfAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "GAS | Attributes")
	TSubclassOf<UGameplayEffect> DefaultAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "GAS | Abilities")
	TObjectPtr<UAbilityConfig> AbilityConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GAS | Attributes")
	TObjectPtr<UCharacterStatConfig> StatConfig;

	UFUNCTION()
	void HandleCombatModeChanged(FGameplayTag NewMode);
	virtual void BeginPlay() override;
	void EndPlay(EEndPlayReason::Type EndPlayReason);
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	void SetupAbilitySystem();
	void ApplyDefaultAttributes();
	virtual void PossessedBy(AController* NewController) override;

private:
	UPROPERTY()
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	UPROPERTY()
	TArray<FGameplayTag> GrantedAbilityTags;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void AddCharacterAbilities();

	UPROPERTY(EditDefaultsOnly, Category = "GAS | Effects")
	TSubclassOf<UGameplayEffect> PassiveAdrenalineGE;

	UPROPERTY(EditDefaultsOnly, Category = "GAS | Effects")
	TSubclassOf<UGameplayEffect> PassiveFlowGaugeGE;
};