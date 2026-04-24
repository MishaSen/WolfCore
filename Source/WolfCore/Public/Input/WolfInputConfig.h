// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "InputAction.h"
#include "WolfInputConfig.generated.h"

// ============================================================================================================================
// Forward Declarations
// ============================================================================================================================

/** Struct mapping an input action asset to its corresponding GameplayTag for enhanced input binding resolution. */
USTRUCT(BlueprintType)
struct FTaggedInputAction
{
	GENERATED_BODY()

	/** Pointer to the InputAction asset defining the input action configuration and trigger parameters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WolfCore|Input")
	const UInputAction* InputAction = nullptr;

	/** GameplayTag used to identify this input action in ability binding and combat mode processing systems. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (Categories = "InputTag"), Category = "WolfCore|Input")
	FGameplayTag InputTag = FGameplayTag();
};

/**
 * DataAsset defining input actions and their associated gameplay tags.
 */
UCLASS()
class WOLFCORE_API UWolfInputConfig : public UDataAsset
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Input Actions
	// ============================================================================================================================

public:
	/** Array of FTaggedInputAction entries defining all input actions and their GameplayTag mappings for this character. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WolfCore|Input")
	TArray<FTaggedInputAction> InputActions;

	/** Searches the input action registry to find an input action matching a given GameplayTag for ability binding resolution. */
	/**
	 * @param Tag The FGameplayTag to search for in the input actions array.
	 * @param bLogNotFound Boolean flag indicating whether to log a warning message if no matching action is found.
	 * @return Pointer to the UInputAction asset matching the provided tag, or nullptr if not found.
	 */
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& Tag, const bool bLogNotFound = false) const;
};