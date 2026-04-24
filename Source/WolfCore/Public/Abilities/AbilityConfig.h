// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "AbilityConfig.generated.h"

// ============================================================================================================================
// Forward Declarations
// ============================================================================================================================

class UGameplayAbility;

/**
 * Describes a single ability with its class, tag, and optional additional tags.
 */
USTRUCT(BlueprintType)
struct FAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag AbilityTag;

	// Optional: Other tags to describe ability
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer AdditionalAbTags;
};

/**
 * DataAsset containing the master list of all abilities for a character.
 */
UCLASS()
class WOLFCORE_API UAbilityConfig : public UDataAsset
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Ability Registry
	// ============================================================================================================================

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<FAbilityInfo> CharacterAbilities;

	UFUNCTION(BlueprintCallable)
	void GetAbilitiesByTag(const FGameplayTag& SearchTag, TArray<FAbilityInfo>& OutAbilities) const;
};