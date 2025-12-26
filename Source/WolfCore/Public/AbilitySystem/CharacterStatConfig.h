// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterStatConfig.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API UCharacterStatConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	float StartingHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	float FlowGauge = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	float Adrenaline = 0.f;
};