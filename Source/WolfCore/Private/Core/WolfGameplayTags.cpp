// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Core/WolfGameplayTags.h"

#include "GameplayTagsManager.h"

FWolfGameplayTags FWolfGameplayTags::GameplayTags;

void FWolfGameplayTags::AddNativeTag(FGameplayTag& Tag, const FName& TagName, const FString& Description)
{
	Tag = UGameplayTagsManager::Get().AddNativeGameplayTag(TagName, Description);
}

void FWolfGameplayTags::InitializeNativeGameplayTags()
{
	// --- Ability Inputs ---
	AddNativeTag(GameplayTags.Input_Ability1, FName("Input.Ability1"), TEXT("Input tag for the first ability."));
	AddNativeTag(GameplayTags.Input_Ability2, FName("Input.Ability2"), TEXT("Input tag for the second ability."));
	AddNativeTag(GameplayTags.Input_Ability3, FName("Input.Ability3"), TEXT("Input tag for the third ability."));
	AddNativeTag(GameplayTags.Input_Ability4, FName("Input.Ability4"), TEXT("Input tag for the fourth ability."));
	AddNativeTag(GameplayTags.Input_Jump, FName("Input.Jump"), TEXT("Input tag for the jump ability."));

	// --- Mouse Inputs ---
	AddNativeTag(GameplayTags.Input_Primary, FName("Input.Primary"), TEXT("Input tag for the primary ability."));
	AddNativeTag(GameplayTags.Input_Secondary, FName("Input.Secondary"), TEXT("Input tag for the secondary ability."));
}