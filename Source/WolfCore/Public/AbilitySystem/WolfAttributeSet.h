// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "WolfAttributeSet.generated.h"

/**
 * AttributeSet defining all combat attributes for Wolf characters.
 * Provides Health, MaxHealth, FlowGauge, and Adrenaline with accessor macros and tag-based lookup.
 */
UCLASS()
class WOLFCORE_API UWolfAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Attributes - Vitality
	// ============================================================================================================================

public:
	/** Current health attribute representing the character's vitality and combat readiness. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UWolfAttributeSet, Health)

	/** Maximum health attribute defining the upper bound for character vitality. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UWolfAttributeSet, MaxHealth)

	// ============================================================================================================================
	// Attributes - Combat Resources
	// ============================================================================================================================

	/** Flow gauge attribute representing the character's combat flow resource for ability activation in TB Mode. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Attributes")
	FGameplayAttributeData FlowGauge;
	ATTRIBUTE_ACCESSORS_BASIC(UWolfAttributeSet, FlowGauge)

	/** Adrenaline attribute representing the character's adrenaline resource for ability activation in RT Mode. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Attributes")
	FGameplayAttributeData Adrenaline;
	ATTRIBUTE_ACCESSORS_BASIC(UWolfAttributeSet, Adrenaline)

	// ============================================================================================================================
	// Attribute System Hooks
	// ============================================================================================================================

	/** Called before an attribute value is modified, allowing pre-modification validation and clamping. */
	/**
	 * @param Attribute The FGameplayAttribute being modified.
	 * @param NewValue Reference to the new attribute value that may be adjusted by this override.
	 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** Called after a GameplayEffect execution modifies attributes, allowing post-modification effects. */
	/**
	 * @param Data The FGameplayEffectModCallbackData containing context about the effect execution.
	 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ============================================================================================================================
	// Tag-Based Lookup
	// ============================================================================================================================

	/** Retrieves a gameplay attribute by its associated GameplayTag for dynamic attribute resolution. */
	/**
	 * @param Tag The FGameplayTag to look up in the tag-to-attribute mapping table.
	 * @return FGameplayAttribute representing the matched attribute, or an invalid attribute if not found.
	 */
	static FGameplayAttribute GetAttributeByTag(const FGameplayTag& Tag);

protected:
	/** Internal map storing GameplayTag to attribute function mappings for dynamic lookup and resolution. */
	static TMap<FGameplayTag, TFunction<FGameplayAttribute()>> TagToAttributeMap;
};