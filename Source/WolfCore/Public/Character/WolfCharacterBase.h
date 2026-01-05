// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
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
class WOLFCORE_API AWolfCharacterBase : public ACharacter, public IAbilitySystemInterface, public ISnapshot
{
	GENERATED_BODY()

#pragma region Presage System
	
public:
	UFUNCTION(BlueprintPure, Category = "Wolf|Presage")
	FTransform GetProjectedTransform(float FutureTimeDelta) const;

	UFUNCTION(BlueprintPure, Category = "Wolf|Presage")
	float GetTimeToNextHitImpact() const;

	void GetPresageCollisionDimensions(float& OutRadius, float& OutHalfHeight) const;
	bool IsInvulnerableAt(float RelativeTime) const;
	class UTBCombatAbility* GetCurrentTBAbility() const;
	
protected:
	FTransform ExtractRootMotionAtTime(UAnimMontage* Montage, float Time) const;

	virtual void CreateSnapshot_Implementation(FActorSnapshot& OutSnapshot) override;

	// NOTE: May need boolean to silence delegates that trigger animations and sounds based on attribute changes
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& InSnapshot) override;

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

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ability System")
	TSubclassOf<UWolfAbilitySystemComponent> AbilitySystemComponentClass;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<UWolfAbilitySystemComponent> ASC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes")
	TObjectPtr<UWolfAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TObjectPtr<UAbilityConfig> AbilityConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Attributes")
	TObjectPtr<UCharacterStatConfig> StatConfig;
	
	virtual void BeginPlay() override;
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
};