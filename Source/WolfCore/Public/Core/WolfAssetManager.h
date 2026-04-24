// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "WolfAssetManager.generated.h"

/**
 * Custom Asset Manager providing singleton access and custom initialization behavior.
 */
UCLASS()
class WOLFCORE_API UWolfAssetManager : public UAssetManager
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Static Accessors
	// ============================================================================================================================

public:
	static UWolfAssetManager& Get();

	// ============================================================================================================================
	// Asset Manager Hooks
	// ============================================================================================================================

	virtual void StartInitialLoading() override;
};