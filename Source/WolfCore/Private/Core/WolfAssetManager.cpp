// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Core/WolfAssetManager.h"

#include "AbilitySystemGlobals.h"
#include "WolfCore/Public/Core/WolfGameplayTags.h"

UWolfAssetManager& UWolfAssetManager::Get()
{
	check(GEngine)
	if (UWolfAssetManager* AssetManager = Cast<UWolfAssetManager>(GEngine->AssetManager))
	{
		return *AssetManager;
	}

	UE_LOG(LogTemp, Fatal, TEXT(
		       "Error: Asset Manager not found. You had ONE job... "
		       "Set 'AssetManagerClassName' in DefaultEngine.ini to '/Script/Wolf.WolfAssetManager'."));
	return *NewObject<UWolfAssetManager>();
}

void UWolfAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	FWolfGameplayTags::InitializeNativeGameplayTags();

	// Initializes global data required for using GAS' targeting features (e.g., FGameplayAbilityTargetData).
	UAbilitySystemGlobals::Get().InitGlobalData();
}