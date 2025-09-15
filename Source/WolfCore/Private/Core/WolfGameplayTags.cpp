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
	AddNativeTag(GameplayTags.Input_Ability_1, FName("Input.Ability.1"), TEXT("Input tag for the first ability."));
	AddNativeTag(GameplayTags.Input_Ability_2, FName("Input.Ability.2"), TEXT("Input tag for the second ability."));
	AddNativeTag(GameplayTags.Input_Ability_3, FName("Input.Ability.3"), TEXT("Input tag for the third ability."));
	AddNativeTag(GameplayTags.Input_Ability_4, FName("Input.Ability.4"), TEXT("Input tag for the fourth ability."));
	AddNativeTag(GameplayTags.Input_Jump, FName("Input.Jump"), TEXT("Input tag for the jump ability."));

	// --- Mouse Inputs ---
	AddNativeTag(GameplayTags.Input_LMB, FName("Input.LMB"), TEXT("Input tag for the left mouse button."));
	AddNativeTag(GameplayTags.Input_RMB, FName("Input.RMB"), TEXT("Input tag for the right mouse button."));
}