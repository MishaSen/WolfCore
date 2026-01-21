// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/CombatModeSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "WolfLevelScript.h"
#include "Core/WolfGameInstance.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "Kismet/GameplayStatics.h"

void UCombatModeSubsystem::SetMode(FGameplayTag NewMode)
{
	if (CurrentMode == NewMode)
	{
		WOLF_LOG(Log, TEXT("Current Mode is already set to %s"), *NewMode.ToString());
		return;
	}
	
	CurrentMode = NewMode;
	WOLF_LOG(Log, TEXT("Current Mode set to %s"), *NewMode.ToString());

	TArray<AActor*> CombatActors;
	UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UAbilitySystemInterface::StaticClass(), CombatActors);

	for (auto Actor : CombatActors)
	{
		if (auto* ASC = Cast<IAbilitySystemInterface>(Actor)->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(WolfTag.InputState_RT);
			ASC->RemoveLooseGameplayTag(WolfTag.InputState_TB);
			ASC->RemoveLooseGameplayTag(WolfTag.InputState_OOC);
			ASC->AddLooseGameplayTag(NewMode);
		}
	}

	WOLF_INFO(TEXT("Global Combat Mode set to %s for %d actors"), *NewMode.ToString(), CombatActors.Num());
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

	if (auto* GameInstance = InWorld.GetGameInstance<UWolfGameInstance>();
		GameInstance && GameInstance->SelectedCombatMode.IsValid())
	{
		SetMode(GameInstance->SelectedCombatMode);
		WOLF_LOG(Log, TEXT("Selected Combat Mode set to %s"), *GameInstance->SelectedCombatMode.ToString());
		
		GameInstance->ClearSelectedCombatMode();
	}
	else // Select the default level mode if no player choice
	{
		const auto* LevelScript = Cast<AWolfLevelScript>(InWorld.GetLevelScriptActor());
		if (!LevelScript)
		{
			WOLF_ERROR(TEXT("LevelScript not found."));
			return;
		}

		SetMode(LevelScript->StartingCombatTag);
		WOLF_LOG(Log, TEXT("Combat Mode set to default level mode: %s"), *LevelScript->StartingCombatTag.ToString());
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