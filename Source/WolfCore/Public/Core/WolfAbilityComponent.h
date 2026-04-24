// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/WolfAbilitySystemComponent.h"
#include "Components/ActorComponent.h"
#include "WolfAbilityComponent.generated.h"

class UCharacterStatConfig;
class UWolfAttributeSet;
class UWolfAbilitySystemComponent;

/**
 * Actor component that owns and manages the Wolf Gameplay Ability System.
 * Handles ability initialization, attribute management, and startup ability granting.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WOLFCORE_API UWolfAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	UWolfAbilityComponent();

	void InitializeAbilitySystem(AActor* InOwner);
	void AddStartupAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities);
	void ApplyDefaultAttributes();

	// ============================================================================================================================
	// Public API - Accessors
	// ============================================================================================================================

	UFUNCTION(BlueprintCallable, Category = "Wolf|Abilities")
	UWolfAbilitySystemComponent* GetWolfASC() const { return CacheASC; }

	FORCEINLINE const TArray<FGameplayAttribute>& GetCachedAttributes() const { return CachedAttributes; }
	FORCEINLINE const UCharacterStatConfig* GetStatConfig() const { return StatConfig; }

	// ============================================================================================================================
	// Internal State
	// ============================================================================================================================

protected:
	UPROPERTY(VisibleInstanceOnly, Category = "Wolf|Abilities|Internal")
	TObjectPtr<UWolfAbilitySystemComponent> CacheASC;

	UPROPERTY(VisibleInstanceOnly, Category = "Wolf|Abilities|Internal")
	TObjectPtr<UWolfAttributeSet> AttributeSet;

	// ============================================================================================================================
	// Configuration
	// ============================================================================================================================

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wolf|Abilities")
	TObjectPtr<UCharacterStatConfig> StatConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wolf|Abilities")
	TSubclassOf<UGameplayEffect> DefaultAttributes;

private:
	UPROPERTY(Transient)
	TArray<FGameplayAttribute> CachedAttributes;
};