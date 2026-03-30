// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/CombatModeSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemComponent.h"
#include "WolfLevelScript.h"
#include "Abilities/Effects/PresageMode.h"
#include "Character/WolfCharacterBase.h"
#include "Core/WolfCombatSettings.h"
#include "Core/WolfGameInstance.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "Engine/AssetManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/CombatModeListener.h"
#include "Kismet/GameplayStatics.h"

void UCombatModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
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

	if (const auto* Settings = GetDefault<UWolfCombatSettings>())
	{
		auto& Streamable = UAssetManager::GetStreamableManager();
		PresageClassLoadHandle = Streamable.RequestAsyncLoad( // Async Load: Set the class once loaded 
			Settings->PresageEffectClass.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &UCombatModeSubsystem::OnPresageEffectLoaded));
	}
}

void UCombatModeSubsystem::OnPresageEffectLoaded()
{
	if (const auto* Settings = GetDefault<UWolfCombatSettings>())
	{
		PresageEffectClass = Settings->PresageEffectClass.Get();
		WOLF_LOG(Log, TEXT("Presage Effect Loaded: %s"), *PresageEffectClass->GetName());
	}
}

void UCombatModeSubsystem::RegisterCombatant(AWolfCharacterBase* Character)
{
	if (Character && !TrackedCombatants.Contains(Character))
	{
		TrackedCombatants.Add(Character);
		ApplyModeToActor(Character, CurrentMode); // Adds Actor to Presage
	}
}

void UCombatModeSubsystem::UnregisterCombatant(AWolfCharacterBase* Character)
{
	TrackedCombatants.RemoveSingleSwap(Character); // Remove() already handles if check
}

FTemporalStates UCombatModeSubsystem::CaptureCurrentWorldState(float Timestamp)
{
	FTemporalStates NewState;
	NewState.WorldTimeAnchor = Timestamp;

	WOLF_LOG(Log, TEXT("=== Starting World Snapshot at Timestamp: %.2f ==="), Timestamp);

	for (auto It = TrackedCombatants.CreateIterator(); It; ++It)
	{
		auto* WolfChar = It->Get();
		if (WolfChar)
		{
			FActorSnapshot ActorState;
			if (WolfChar->Implements<USnapshot>())
			{
				ISnapshot::Execute_CreateSnapshot(WolfChar, ActorState);
			}
			else
			{
				WolfChar->CreateSnapshot_Implementation(ActorState);
			}
			WOLF_LOG(Verbose, TEXT("Snapshotted [%s] at %s"), *WolfChar->GetName(), *ActorState.Location.ToString());
			NewState.ActorStates.Add(WolfChar, ActorState);
		}
		else
		{
			It.RemoveCurrent();
		}
	}
	WOLF_LOG(Log, TEXT("Snapshot Complete. Captured %d combatants."), NewState.ActorStates.Num());
	return NewState;
}

void UCombatModeSubsystem::SetMode(FGameplayTag NewMode)
{
	if (CurrentMode == NewMode) return;
	CurrentMode = NewMode;

	const auto* World = GetWorld();
	if (IsValid(World)) UGameplayStatics::SetGlobalTimeDilation(World, GetDilationForMode(NewMode));

	if (NewMode == WolfTag.InputState_TB)
	{
		bIsInTB = true;
		MasterStartSnapshot = CaptureCurrentWorldState(World->GetTimeSeconds());
		WOLF_LOG(Log, TEXT("TB started. Master Snapshot captured for %d actors."), MasterStartSnapshot.ActorStates.Num());

		GenerateFutureState(MaxTimelineDuration);
		ScrubTimeline(0.f);
	}
	else
	{
		MasterStartSnapshot.ActorStates.Empty();
		bIsInTB = false;
	}
	
	OnCombatModeChanged.Broadcast(NewMode);
}

void UCombatModeSubsystem::SwitchCombatMode()
{
	const auto NewMode = CurrentMode == WolfTag.InputState_RT
		                     ? WolfTag.InputState_TB
		                     : WolfTag.InputState_RT;

	SetMode(NewMode);
}

void UCombatModeSubsystem::ScrubTimeline(float NewTime)
{
	const auto ClampedTime = FMath::Clamp(NewTime, 0.f, MaxTimelineDuration);
	if (FMath::IsNearlyEqual(ClampedTime, CurrentTimelineTime)) return; // No time change; no snapshot change
	CurrentTimelineTime = ClampedTime;

	for (auto It = TrackedCombatants.CreateIterator(); It; ++It)
	{
		auto* WolfChar = It->Get();
		if (IsValid(WolfChar))
		{
			const auto* BakedFrame = WolfChar->GetSnapshotAtTime(CurrentTimelineTime);
			if (BakedFrame) WolfChar->RestoreSnapshot_Implementation(*BakedFrame);
		}
		else It.RemoveCurrent();
	}
	WOLF_LOG(Log, TEXT("Timeline Scrubbed to : %.2fs /  %.2fs"), CurrentTimelineTime, MaxTimelineDuration);
}

void UCombatModeSubsystem::HandlePresageDrainEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode)
{
	auto* WolfChar = Cast<AWolfCharacterBase>(ASC->GetAvatarActor());
	if (!WolfChar) return;

	const bool bShouldHaveEffect = CurrentActorMode == WolfTag.InputState_TB;

	// If GE and GTag match, skip. We want to remove or add the GE if there's a mismatch.
	if (bShouldHaveEffect == WolfChar->PresageEffectHandle.IsValid()) return;

	if (bShouldHaveEffect && PresageEffectClass)
	{
		auto Context = ASC->MakeEffectContext();
		Context.AddInstigator(ASC->GetOwner(), ASC->GetOwner());

		const auto Spec = ASC->MakeOutgoingSpec(PresageEffectClass, 1.f, Context);
		if (!Spec.IsValid()) return;

		WolfChar->PresageEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	else
	{
		ASC->RemoveActiveGameplayEffect(WolfChar->PresageEffectHandle);
		WolfChar->PresageEffectHandle.Invalidate();
	}
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

	static const auto ModeTags = []{
		FGameplayTagContainer Container;
		Container.AddTag(FWolfGameplayTags::Get().InputState_RT);
		Container.AddTag(FWolfGameplayTags::Get().InputState_TB);
		Container.AddTag(FWolfGameplayTags::Get().InputState_OOC);
		return Container;
	}();

	ASC->RemoveLooseGameplayTags(ModeTags);
	WOLF_LOG(Log, TEXT("Removing Mode Tags: %s"), *ModeTags.ToString());
	ASC->AddLooseGameplayTag(ActualModeForActor);
	WOLF_LOG(Log, TEXT("Adding Mode Tag %s for %s"), *ActualModeForActor.ToString(), *Combatant->GetName());

	if (bIsPlayer)
	{
		HandlePresageDrainEffect(ASC, ActualModeForActor);
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

float UCombatModeSubsystem::GetDilationForMode(const FGameplayTag& Mode)
{
	const auto* Settings = GetDefault<UWolfCombatSettings>();
	if (const auto* Dilation = Settings->ModeTimeDilationMap.Find(Mode)) return *Dilation;

	WOLF_WARN(TEXT("Dilation Map missing tag: %s. Defaulting to 1.f"), *Mode.ToString());
	return 1.f;
}

void UCombatModeSubsystem::GenerateFutureState(float Duration)
{
	for (auto It = TrackedCombatants.CreateIterator(); It; ++It)
	{
		if (auto* WolfChar = It->Get())
		{
			WolfChar->ClearPredictionBuffer(MaxTimelineDuration);

			if (auto* MoveComp = WolfChar->GetCharacterMovement())
			{
				if (!WolfChar->GetCurrentMontage()) MoveComp->StopMovementImmediately();
			}
		}
		else It.RemoveCurrent();
	}

	constexpr float Step = WolfSimConfig::Step;
	const int32 TotalSteps = FMath::CeilToInt(Duration / Step);

	WOLF_LOG(Log, TEXT("Baking Future: %d steps over %.2fs"), TotalSteps, Duration);

	for (int32 i = 0; i < TotalSteps; ++i)
	{
		for (auto& Combatant : TrackedCombatants)
		{
			if (auto* WolfChar = Combatant.Get()) WolfChar->SimulateTick(Step);
		}
	}
}