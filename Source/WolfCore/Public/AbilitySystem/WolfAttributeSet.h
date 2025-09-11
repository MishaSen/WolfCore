// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "WolfAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API UWolfAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UWolfAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData FlowGauge;
	ATTRIBUTE_ACCESSORS_BASIC(UWolfAttributeSet, FlowGauge)
};
