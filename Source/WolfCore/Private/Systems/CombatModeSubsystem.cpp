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
#include "Presage/PresageSubsystem.h"

void UCombatModeSubsystem::RegisterCombatListener(AActor* Combatant)
{
	if (!IsValid(Combatant) || Combatants.Contains(Combatant)) return;

	Combatants.Add(Combatant);
	ApplyModeToActor(Combatant, CurrentMode);
	WOLF_LOG(Log, TEXT("Combatant registered: %s"), *Combatant->GetName());
}

void UCombatModeSubsystem::UnregisterCombatListener(const AActor* Combatant)
{
	if (IsValid(Combatant))
	{
		Combatants.Remove(Combatant);
		WOLF_LOG(Log, TEXT("Combatant unregistered: %s"), *Combatant->GetName());
	}
}

void UCombatModeSubsystem::ApplyModeToActor(AActor* Combatant, FGameplayTag NewMode)
{
	const auto* ASI = Cast<IAbilitySystemInterface>(Combatant);
	auto* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;
	if (!ASC) return;

	// Enemies and unlinked allies don't need to switch
	const auto* Pawn = Cast<APawn>(Combatant);
	const bool bIsPlayer = Pawn && Pawn->IsPlayerControlled();
	const bool bHasLink = ASC->HasMatchingGameplayTag(WolfTag.Status_Link);
	const auto ActualModeForActor = bIsPlayer || bHasLink ? NewMode : WolfTag.InputState_RT;

	ASC->RemoveLooseGameplayTag(WolfTag.InputState_RT);
	ASC->RemoveLooseGameplayTag(WolfTag.InputState_TB);
	ASC->RemoveLooseGameplayTag(WolfTag.InputState_OOC);
	ASC->AddLooseGameplayTag(ActualModeForActor);

	if (Combatant->Implements<UCombatModeListener>())
	{
		ICombatModeListener::Execute_OnCombatModeChanged(Combatant, ActualModeForActor);
	}
}

void UCombatModeSubsystem::UpdateCombatantModeTags(FGameplayTag NewMode)
{
	for (auto Iterator = Combatants.CreateIterator(); Iterator; ++Iterator)
	{
		if (!IsValid(*Iterator))
		{
			Iterator.RemoveCurrent();
			continue;
		}
		ApplyModeToActor(*Iterator, NewMode);
	}
	WOLF_INFO(TEXT("Combat Mode set to %s for %d actors"), *NewMode.ToString(), Combatants.Num());
}

void UCombatModeSubsystem::SetMode(FGameplayTag NewMode)
{
	if (CurrentMode == NewMode) return;
	CurrentMode = NewMode;

	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), GetDilationForMode(NewMode));

	UpdateCombatantModeTags(NewMode);

	auto* Presage = GetWorld()->GetSubsystem<UPresageSubsystem>();
	if (Presage)
	{
		if (NewMode == WolfTag.InputState_TB)
		{
			Presage->StartLoop();
		}
		else
		{
			Presage->StopLoop();
		}
	}
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

	ModeTimeDilationMap.Add(WolfTag.InputState_TB, 0.f);
	ModeTimeDilationMap.Add(WolfTag.InputState_RT, 1.f);
	ModeTimeDilationMap.Add(WolfTag.InputState_OOC, 1.f);

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
