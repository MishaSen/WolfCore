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

	/** The Gameplay Ability subclass assigned to this ability entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|Abilities")
	TSubclassOf<UGameplayAbility> Ability;

	/** The GameplayTag used to identify and query this ability in the system. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|Abilities")
	FGameplayTag AbilityTag;

	/** Container of additional GameplayTags that provide supplementary information about this ability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|Abilities")
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
	/** Master array of FAbilityInfo entries defining all abilities granted to a character. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|Abilities")
	TArray<FAbilityInfo> CharacterAbilities;

	/** Queries the ability registry for all abilities matching a given GameplayTag and outputs them. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Get Abilities By Tag"), Category = "WolfCore|Abilities")
	/**
	 * @param SearchTag The GameplayTag to search for in the ability registry.
	 * @param OutAbilities Output array populated with FAbilityInfo entries matching the search tag.
	 */
	void GetAbilitiesByTag(const FGameplayTag& SearchTag, TArray<FAbilityInfo>& OutAbilities) const;
};