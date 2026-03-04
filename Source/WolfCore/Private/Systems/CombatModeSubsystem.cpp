// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/CombatModeSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemComponent.h"
#include "WolfLevelScript.h"
#include "Abilities/Effects/PresageMode.h"
#include "Core/WolfCombatSettings.h"
#include "Core/WolfFunctionLibrary.h"
#include "Core/WolfGameInstance.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "Interfaces/CombatModeListener.h"
#include "Kismet/GameplayStatics.h"
#include "Presage/PresageSubsystem.h"

void UCombatModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UPresageSubsystem>();
	Super::Initialize(Collection);
}

void UCombatModeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	InitializeSubsystemDefaults();

	if (TryLoadPlayerSelectedMode()) return;
	ApplyDefaultLevelMode();
}

void UCombatModeSubsystem::InitializeSubsystemDefaults()
{
	WolfTag = FWolfGameplayTags::Get();
	ModeTimeDilationMap.Add(WolfTag.InputState_TB, 0.f);
	ModeTimeDilationMap.Add(WolfTag.InputState_RT, 1.f);
	ModeTimeDilationMap.Add(WolfTag.InputState_OOC, 1.f);

	CachedPresage = UWolfFunctionLibrary::GetWorldSubsystem<UPresageSubsystem>(GetWorld());

	if (const auto* Settings = GetDefault<UWolfCombatSettings>())
	{
		PresageEffectClass = Settings->PresageEffectClass.LoadSynchronous();

		if (!PresageEffectClass)
		{
			WOLF_ERROR(TEXT("PresageEffectClass not found in Project Settings"));
		}
	}
}

void UCombatModeSubsystem::SetMode(FGameplayTag NewMode)
{
	if (CurrentMode == NewMode) return;
	CurrentMode = NewMode;

	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), GetDilationForMode(NewMode));
	UpdateCombatantModeTags(NewMode);

	if (CachedPresage)
	{
		const bool bIsTurnBased = NewMode == WolfTag.InputState_TB;
		bIsTurnBased ? CachedPresage->StartLoop() : CachedPresage->StopLoop();
	}
}

void UCombatModeSubsystem::SwitchCombatMode()
{
	const auto NewMode = CurrentMode == WolfTag.InputState_RT
		                     ? WolfTag.InputState_TB
		                     : WolfTag.InputState_RT;

	SetMode(NewMode);
}

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

void UCombatModeSubsystem::HandlePlayerPresageEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode)
{
	const bool bShouldHaveEffect = CurrentActorMode == WolfTag.InputState_TB;
	const bool bHasActivePresage = PresageEffectHandle.IsValid();

	// If GE and GTag match, skip. We want to remove or add the GE if there's a mismatch.
	if (bShouldHaveEffect == bHasActivePresage) return;

	if (bShouldHaveEffect && PresageEffectClass)
	{
		auto Context = ASC->MakeEffectContext();
		Context.AddInstigator(ASC->GetOwner(), ASC->GetOwner());

		const auto Spec = ASC->MakeOutgoingSpec(PresageEffectClass, 1.f, Context);
		if (Spec.IsValid())
		{
			PresageEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
		return;
	}
	
	ASC->RemoveActiveGameplayEffect(PresageEffectHandle);
	PresageEffectHandle.Invalidate();
}

void UCombatModeSubsystem::ApplyModeToActor(AActor* Combatant, FGameplayTag NewMode)
{
	auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Combatant);
	if (!ASC) return;

	// Enemies and unlinked allies don't need to switch
	const auto* Pawn = Cast<APawn>(Combatant);
	const bool bIsPlayer = Pawn && Pawn->IsPlayerControlled();
	const bool bCanChangeMode = bIsPlayer || ASC->HasMatchingGameplayTag(WolfTag.Status_Link);
	const auto ActualModeForActor = bCanChangeMode ? NewMode : WolfTag.InputState_RT;

	ASC->RemoveLooseGameplayTag(WolfTag.InputState_RT);
	ASC->RemoveLooseGameplayTag(WolfTag.InputState_TB);
	ASC->RemoveLooseGameplayTag(WolfTag.InputState_OOC);
	ASC->AddLooseGameplayTag(ActualModeForActor);

	if (bIsPlayer)
	{
		HandlePlayerPresageEffect(ASC, ActualModeForActor);
	}

	if (Combatant->Implements<UCombatModeListener>())
	{
		ICombatModeListener::Execute_OnCombatModeChanged(Combatant, ActualModeForActor);
	}
}

bool UCombatModeSubsystem::TryLoadPlayerSelectedMode()
{
	auto* GI = GetWorld()->GetGameInstance<UWolfGameInstance>();
	if (GI && GI->SelectedCombatMode.IsValid())
	{
		SetMode(GI->SelectedCombatMode);
		WOLF_LOG(Log, TEXT("Selected Combat Mode set to %s"), *GI->SelectedCombatMode.ToString());

		GI->ClearSelectedCombatMode();
		return true;
	}
	return false;
	/* TODO: We have the logic to influence combat mode through player input.
	 * Now we need to implement a way for players to input the desired mode. */
}

void UCombatModeSubsystem::ApplyDefaultLevelMode()
{
	const auto* LevelScript = Cast<AWolfLevelScript>(GetWorld()->GetLevelScriptActor());
	if (LevelScript)
	{
		SetMode(LevelScript->StartingCombatTag);
		WOLF_LOG(Log, TEXT("Combat Mode defaulting to Level Mode: %s"), *LevelScript->StartingCombatTag.ToString());
	}
	else
	{
		WOLF_ERROR(TEXT("LevelScript not found - No default mode set"));
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

float UCombatModeSubsystem::GetDilationForMode(const FGameplayTag& Mode) const
{
	const auto* ModeDilation = ModeTimeDilationMap.Find(Mode);
	if (!ModeDilation)
		WOLF_WARN(TEXT("Dilation Map missing tag: %s"), *Mode.ToString());
	return ModeDilation ? *ModeDilation : 1.f;
}
