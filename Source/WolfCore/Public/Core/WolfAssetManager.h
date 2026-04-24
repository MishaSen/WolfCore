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
	/** Retrieves the singleton instance of the Wolf Asset Manager for global asset management operations. */
	static UWolfAssetManager& Get();

	// ============================================================================================================================
	// Asset Manager Hooks
	// ============================================================================================================================

	/** Initiates the initial asset loading pipeline, preloading core resources required for gameplay initialization. Overrides UAssetManager::StartInitialLoading(). */
	virtual void StartInitialLoading() override;
};