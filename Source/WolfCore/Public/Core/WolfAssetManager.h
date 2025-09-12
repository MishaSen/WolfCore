// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "WolfAssetManager.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API UWolfAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	static UWolfAssetManager& Get();
	virtual void StartInitialLoading() override;
};
