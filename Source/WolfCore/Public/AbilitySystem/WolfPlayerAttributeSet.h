// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WolfAttributeSetBase.h"
#include "WolfPlayerAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API UWolfPlayerAttributeSet : public UWolfAttributeSetBase
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData FlowGauge;
	ATTRIBUTE_ACCESSORS_BASIC(UWolfPlayerAttributeSet, FlowGauge)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Adrenaline;
	ATTRIBUTE_ACCESSORS_BASIC(UWolfPlayerAttributeSet, Adrenaline)
};
