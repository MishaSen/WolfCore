// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "AbilityConfig.generated.h"

class UGameplayAbility;

USTRUCT(BlueprintType)
struct FAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> Ability;

	UPROPERTY(editAnywhere, BlueprintReadOnly)
	FGameplayTag AbilityTag;

	// Optional: Other tags to describe ability
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer AdditionalAbTags;
};

/**
 * 
 */
UCLASS()
class WOLFCORE_API UAbilityConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// Master list of all abilities
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<FAbilityInfo> CharacterAbilities;

	void GetAbilitiesByTag(const FGameplayTag& SearchTag, TArray<FAbilityInfo>& OutAbilities) const;
};