// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Core/WolfAssetManager.h"

#include "AbilitySystemGlobals.h"
#include "Debug/WolfDebug.h"
#include "WolfCore/Public/Core/WolfGameplayTags.h"

UWolfAssetManager& UWolfAssetManager::Get()
{
	check(GEngine)
	if (UWolfAssetManager* AssetManager = Cast<UWolfAssetManager>(GEngine->AssetManager))
	{
		WOLF_LOG(Log, TEXT("WolfAssetManager singleton obtained."));
		return *AssetManager;
	}

	WOLF_ERROR(TEXT("Error: Asset Manager not found. You had one job... "
				 "Set 'AssetManagerClassName' in DefaultEngine.ini to '/Script/WolfCore.WolfAssetManager'."));
	return *NewObject<UWolfAssetManager>();
}

void UWolfAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	FWolfGameplayTags::InitializeNativeGameplayTags();

	// Initializes global data required for using GAS' targeting features (e.g., FGameplayAbilityTargetData).
	UAbilitySystemGlobals::Get().InitGlobalData();
}