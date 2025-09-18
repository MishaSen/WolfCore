// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"

#include "WolfCharacterBase.generated.h"

class UGameplayAbility;
class UWolfAbilitySystemComponent;
class UWolfAttributeSet;
/**
 * 
 */
class UGameplayEffect;

UCLASS()
class WOLFCORE_API AWolfCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AWolfCharacterBase();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Abilities")
	FGameplayAbilitySpecHandle GetAbilitySpecHandle(const TSubclassOf<UGameplayAbility>& AbilityClass) const;

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void QueueAbility(const FGameplayAbilitySpecHandle SpecHandle,
	                  const float ScheduledTime,
	                  const FGameplayTag AbilityTag);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UWolfAbilitySystemComponent* ASC;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<UGameplayEffect> DefaultAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TMap<TSubclassOf<UGameplayAbility>, FGameplayTag> TBAbilities;
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

private:
	UPROPERTY()
	UWolfAttributeSet* AtSet;

	UPROPERTY()
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void AddCharacterAbilities();
};