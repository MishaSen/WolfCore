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
	/** Default constructor for UWolfAbilityComponent, initializing the ability system component. */
	UWolfAbilityComponent();

	/** Initializes the ability system by linking this component to its owning actor's ASC. */
	/**
	 * @param InOwner Pointer to the AActor that owns this ability component.
	 */
	void InitializeAbilitySystem(AActor* InOwner);

	/** Grants startup abilities to the character's ASC for activation during gameplay. */
	/**
	 * @param Abilities Array of ability subclasses to grant and activate on this component.
	 */
	void AddStartupAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities);

	/** Applies default attribute values from the character stat configuration to the attribute set. */
	void ApplyDefaultAttributes();

	// ============================================================================================================================
	// Public API - Accessors
	// ============================================================================================================================

	/** Retrieves the cached Ability System Component (ASC) for Gameplay Ability System interactions. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Get Wolf ASC"), Category = "WolfCore|Abilities")
	/**
	 * @return Pointer to the UWolfAbilitySystemComponent, or nullptr if not initialized.
	 */
	UWolfAbilitySystemComponent* GetWolfASC() const { return CacheASC; }

	/** Returns the array of cached gameplay attributes for runtime queries and modifications. */
	FORCEINLINE const TArray<FGameplayAttribute>& GetCachedAttributes() const { return CachedAttributes; }

	/** Provides access to the character stat configuration asset used for attribute initialization. */
	FORCEINLINE const UCharacterStatConfig* GetStatConfig() const { return StatConfig; }

	// ============================================================================================================================
	// Internal State
	// ============================================================================================================================

protected:
	/** Cached reference to the Ability System Component (ASC) for performance optimization during ability operations. */
	UPROPERTY(VisibleInstanceOnly, Category = "WolfCore|Abilities|Internal")
	TObjectPtr<UWolfAbilitySystemComponent> CacheASC;

	/** Strong reference to the character's attribute set containing vitality and combat resource values. */
	UPROPERTY(VisibleInstanceOnly, Category = "WolfCore|Abilities|Internal")
	TObjectPtr<UWolfAttributeSet> AttributeSet;

	// ============================================================================================================================
	// Configuration
	// ============================================================================================================================

protected:
	/** Character stat configuration asset that defines default attribute values mapped by GameplayTag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|Abilities")
	TObjectPtr<UCharacterStatConfig> StatConfig;

	/** Subclass of GameplayEffect applied to the character for default attribute initialization. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|Abilities")
	TSubclassOf<UGameplayEffect> DefaultAttributes;

private:
	/** Array of cached gameplay attributes for quick access during ability resolution and combat calculations. */
	UPROPERTY(Transient)
	TArray<FGameplayAttribute> CachedAttributes;
};