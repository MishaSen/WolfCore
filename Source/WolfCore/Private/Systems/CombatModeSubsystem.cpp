// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/CombatModeSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemComponent.h"
#include "WolfLevelScript.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"

void UCombatModeSubsystem::SetMode(FGameplayTag NewMode)
{
	if (CurrentMode == NewMode)
	{
		WOLF_LOG(Log, TEXT("Current Mode is already set to %s"), *NewMode.ToString());
		return;
	}
	
	CurrentMode = NewMode;
	WOLF_LOG(Log, TEXT("Current Mode set to %s"), *NewMode.ToString());

	if (PlayerASC)
	{
		WOLF_LOG(Log, TEXT("PlayerASC found, clearing old tags"));
	}

	FGameplayTagContainer TagsToRemove;
	TagsToRemove.AddTag(WolfTag.InputState_RT);
	TagsToRemove.AddTag(WolfTag.InputState_TB);
	TagsToRemove.AddTag(WolfTag.InputState_OOC);

	for (const auto& Tag : TagsToRemove)
	{
		if (!Tag.MatchesTag(WolfTag.InputState)) continue;

		if (PlayerASC->HasMatchingGameplayTag(Tag))
		{
			PlayerASC->RemoveLooseGameplayTag(Tag);
			
			WOLF_INFO(TEXT("CombatMode cleared from %s"), *Tag.ToString());
		}
	}

	PlayerASC->AddLooseGameplayTag(NewMode);
	WOLF_INFO(TEXT("CombatMode set to %s"), *NewMode.ToString());
}

void UCombatModeSubsystem::SwitchCombatMode()
{
	const auto NewMode = CurrentMode == WolfTag.InputState_RT
		                     ? WolfTag.InputState_TB
		                     : WolfTag.InputState_RT;

	SetMode(NewMode);
}

void UCombatModeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	WolfTag = FWolfGameplayTags::Get();
	PlayerASC = GetPlayerASC();

	const auto* LevelScript = Cast<AWolfLevelScript>(InWorld.GetLevelScriptActor());
	if (!LevelScript)
	{
		WOLF_ERROR(TEXT("Error: LevelScript not found."));
		return;
	}
	else
	{
		WOLF_INFO( TEXT( "LevelScript found: %s" ), *LevelScript->GetName());
		SetMode(LevelScript->StartingCombatTag);
	}
}

UAbilitySystemComponent* UCombatModeSubsystem::GetPlayerASC() const
{
	const auto* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return nullptr;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return nullptr;

	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
}