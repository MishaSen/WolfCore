// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/CombatModeSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/WolfCharacterBase.h"
#include "Core/WolfAbilityComponent.h"
#include "Core/WolfSnapshotComponent.h"
#include "Misc/App.h"
#include "WolfLevelScript.h"
#include "Core/WolfPresageSimulator.h"
#include "Core/WolfCombatSettings.h"
#include "Core/WolfGameInstance.h"
#include "Core/WolfGameplayTags.h"
#include "Core/WolfPresageComponent.h"
#include "Core/WolfPlayerController.h"
#include "Presage/PresageOrchestrator.h"
#include "Abilities/BaseCombatAbility.h"
#include "Abilities/Effects/ResourceGainEffect.h"
#include "AbilitySystem/WolfAttributeSet.h"
#include "Debug/WolfDebug.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffectTypes.h"
#include "Interfaces/IWolfCombatant.h"
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

	// Resource telemetry (ResourceLoop stage 1): try the lazy bind immediately — no-ops until the
	// player pawn exists (possession order isn't guaranteed); Tick/ApplyResourceGainToPlayer
	// retry it until it succeeds.
	EnsureResourceTelemetryBound();
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

void UCombatModeSubsystem::RegisterCombatant(const TScriptInterface<IWolfCombatant>& Combatant)
{
	if (!Combatant.GetInterface()) return;

	const auto* Actor = Cast<AActor>(Combatant.GetObject());
	if (!Actor) return;
	
	// Avoid duplicate entries in the TrackedCombatants array.
	for (const auto& Existing : TrackedCombatants)
	{
		if (Existing.GetObject() == Actor) return;
	}

	TrackedCombatants.Add(Combatant);

	ApplyModeToActor(Combatant, CurrentMode); // Adds Actor to Presage
}

void UCombatModeSubsystem::UnregisterCombatant(const TScriptInterface<IWolfCombatant>& Combatant)
{
	// Remove matching interface reference.
	for (auto It = TrackedCombatants.CreateIterator(); It; ++It)
	{
		if (It->GetObject() == Combatant.GetObject())
		{
			It.RemoveCurrent();
			return;
		}
	}
}

FTemporalStates UCombatModeSubsystem::CaptureCurrentWorldState(float Timestamp)
{
	FTemporalStates NewState;
	NewState.WorldTimeAnchor = Timestamp;

	WOLF_LOG(Log, TEXT("=== Starting World Snapshot at Timestamp: %.2f ==="), Timestamp);

	for (auto It = TrackedCombatants.CreateIterator(); It; ++It)
	{
		auto& Combatant = *It;
		if (!Combatant.GetInterface() || !IsValid(Combatant.GetObject()))
		{
			It.RemoveCurrent();
			continue;
		}

		auto* CombatantActor = Cast<AActor>(Combatant.GetObject());
		if (!CombatantActor) continue;

		FActorSnapshot ActorState;
		ISnapshot::Execute_CreateSnapshot(CombatantActor, ActorState);
		WOLF_LOG(Log, TEXT("Snapshotted [%s] at %s"), *CombatantActor->GetName(), *ActorState.Location.ToString());
		NewState.ActorStates.Add(CombatantActor, ActorState);
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
		TBPhase = ETBPhase::Planning;
		MasterStartSnapshot = CaptureCurrentWorldState(World->GetTimeSeconds());

		// Apply new mode tags to all tracked combatants.
		for (auto& Combatant : TrackedCombatants)
		{
			if (Combatant.GetInterface() && IsValid(Combatant.GetObject()))
			{
				ApplyModeToActor(Combatant, NewMode);
			}
		}
		
		WOLF_LOG(Log, TEXT("TB started. Master Snapshot captured for %d actors."), MasterStartSnapshot.ActorStates.Num());

		// Reset to a sentinel value so ScrubTimeline(0.f) is not skipped by the IsNearlyEqual check.
		CurrentTimelineTime = -1.f;

		// Set the step size used for all PredictionBuffer index math during this bake.
		if (const auto* Settings = GetDefault<UWolfCombatSettings>())
		{
			BakedStepSize = FMath::Max(Settings->PresageSimulationStep, 0.01f);
		}

		// Seeded once per TB entry, logged so a bad plan is reproducible. Re-initialized (not
		// re-randomized) at the top of every planning pass, including this one and every
		// subsequent ReBakeTimeline this session — see PresageDeterminism.md.
		PresageSessionSeed = FMath::Rand();
		WOLF_LOG(Log, TEXT("[PRESAGE] Session seed: %d"), PresageSessionSeed);
		PresageRandomStream.Initialize(PresageSessionSeed);

		auto Plan = FPresageOrchestrator::RunPlanning(this, TrackedCombatants, MaxTimelineDuration);
		FPresageOrchestrator::ResolveInterrupts(Plan, PresageRandomStream);
		FPresageOrchestrator::DistributePlan(TrackedCombatants, Plan);

		BakeImpactLedger.Empty();
		DamageExitCursor = 0;
		LedgerApplicationCursor = 0;
		FWolfPresageSimulator::ExecuteFutureBake(TrackedCombatants, MaxTimelineDuration, BakedStepSize);
		ScrubTimeline(0.f);
	}
    else
    {
        if (TBPhase == ETBPhase::Planning)
        {
            // Manual mode switch during Planning, not via LockInPlan/ExitTB — treated as
            // abandoning TB. Nothing real has happened yet during Planning, so restoring to
            // TB-entry state via ScrubTimeline(0.f) leaves no baked state leaking into RT.
            // Resource consequences of abandoning are undefined until ResourceLoop lands.
            ScrubTimeline(0.f);
        }
        // If TBPhase is already None, ExitTB() set it before calling this SetMode(RT) itself —
        // this branch must NOT re-scrub in that case, since execution already played out to
        // wherever it stopped and that state must be left alone.
        TBPhase = ETBPhase::None;
        MasterStartSnapshot.ActorStates.Empty();
        bIsInTB = false;
    }

    // Apply input mapping context directly to ensure controls work regardless of tag event firing.
    if (auto* PC = GetWorld()->GetFirstPlayerController())
    {
        if (auto* WolfPC = Cast<AWolfPlayerController>(PC))
        {
            WolfPC->ApplyInputMappingForMode(NewMode);
        }
    }

    OnCombatModeChanged.Broadcast(NewMode);
}

void UCombatModeSubsystem::SwitchCombatMode()
{
	const auto NewMode = CurrentMode == WolfTag.InputState_RT
	                     ? WolfTag.InputState_TB
	                     : WolfTag.InputState_RT;
	WOLF_INFO(TEXT("Current Mode: %s. Switching to %s"), *CurrentMode.ToString(), *NewMode.ToString());

	SetMode(NewMode);
}

void UCombatModeSubsystem::Tick(float DeltaTime)
{
	if (TBPhase != ETBPhase::Executing) return;

	EnsureResourceTelemetryBound();

	// Unscaled real time — immune to global dilation, which stays 0 throughout all of TB
	// (including Executing; see the overview's "TB has two phases" model). Execution playback
	// IS the bake, played forward — exactness is guaranteed by construction.
	ExecutionClock += FApp::GetDeltaTime();

	ScrubTimeline(ExecutionClock);

	// Real application runs before the damage-exit check below, so a hard exit (if any) fires
	// from real applied state rather than merely presentational buffer data.
	ApplyDueLedgerImpacts(ExecutionClock);

	TArray<TWeakObjectPtr<AActor>> DamageVictims;
	if (CheckExecutionDamageExit(ExecutionClock, DamageVictims))
	{
		ExitTB(ETBExitReason::DamageTaken, DamageVictims);
		return;
	}

	if (CheckEndingAction(ExecutionClock))
	{
		ExitTB(ETBExitReason::EndingAction);
		return;
	}

	if (ExecutionClock >= MaxTimelineDuration)
	{
		ExitTB(ETBExitReason::PlanCompleted);
		return;
	}
}

void UCombatModeSubsystem::LockInPlan()
{
	if (!bIsInTB || TBPhase != ETBPhase::Planning)
	{
		WOLF_WARN(TEXT("[PRESAGE] LockInPlan called outside Planning phase — ignored."));
		return;
	}

	// UI-layer validation ("every free linked ally has an action queued") happens before this is
	// called — there is no ally roster to check here yet, and this function deliberately does not
	// invent one.
	OnLockInFlowDeduction();

	ScrubTimeline(0.f);
	ExecutionClock = 0.f;
	DamageExitCursor = 0;
	LedgerApplicationCursor = 0;
	TBPhase = ETBPhase::Executing;
	WOLF_LOG(Log, TEXT("[PRESAGE] TB locked in — execution playback started."));
}

void UCombatModeSubsystem::ExitTB(ETBExitReason Reason, const TArray<TWeakObjectPtr<AActor>>& Victims)
{
	if (!bIsInTB) return;

	// Set before SetMode(RT) below, so SetMode's Planning-abandon branch does not re-fire and
	// re-scrub over state that execution already played out to.
	TBPhase = ETBPhase::None;

	switch (Reason)
	{
	case ETBExitReason::DamageTaken:
	{
		const auto* Settings = GetDefault<UWolfCombatSettings>();
		if (Settings && !Settings->TBHardExitDebuffClass.IsNull())
		{
			// Synchronous load: this is a rare, one-off event (a hard exit), not a per-tick path,
			// so a small load hitch here is an acceptable trade-off against the async-loading
			// machinery InitializeSubsystemDefaults already uses for the (per-mode) drain effect.
			if (auto* DebuffClass = Settings->TBHardExitDebuffClass.LoadSynchronous())
			{
				for (const auto& Victim : Victims)
				{
					auto* VictimActor = Victim.Get();
					if (!IsValid(VictimActor)) continue;

					auto* VictimASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(VictimActor);
					if (!VictimASC) continue;

					const auto Context = VictimASC->MakeEffectContext();
					const auto SpecHandle = VictimASC->MakeOutgoingSpec(DebuffClass, 1.f, Context);
					if (SpecHandle.IsValid())
					{
						VictimASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
					}
				}
			}
		}
		else
		{
			WOLF_LOG(Log, TEXT("[PRESAGE] TB hard-exit: no TBHardExitDebuffClass configured — skipping debuff application."));
		}
		WOLF_LOG(Log, TEXT("[PRESAGE] TB exiting: DamageTaken (%d victim(s))."), Victims.Num());
		break;
	}
	case ETBExitReason::EndingAction:
		// Bonus effects are explicitly TBD per vision ("exact shape TBD") — recognized, not
		// implemented. Log only.
		WOLF_LOG(Log, TEXT("[PRESAGE] TB exiting: EndingAction (bonus effects not yet implemented)."));
		break;
	case ETBExitReason::PlanCompleted:
		WOLF_LOG(Log, TEXT("[PRESAGE] TB exiting: PlanCompleted."));
		break;
	}

	// World state at exit = wherever playback stopped (restored presentational state + whatever
	// real application has occurred — fully real only after stage 3). SetMode's non-TB branch
	// sees TBPhase already None (set above) and does not re-scrub.
	SetMode(WolfTag.InputState_RT);
}

void UCombatModeSubsystem::ApplyDueLedgerImpacts(float InExecutionClock)
{
	while (LedgerApplicationCursor < BakeImpactLedger.Num())
	{
		const FPresageImpactEntry& Entry = BakeImpactLedger[LedgerApplicationCursor];
		if (Entry.ImpactTime > InExecutionClock) break; // ledger is time-ordered

		++LedgerApplicationCursor;

		if (!Entry.bConnected) continue;

		AActor* Attacker = Entry.Attacker.Get();
		AActor* Victim = Entry.Victim.Get();
		if (!IsValid(Attacker)) continue;

		auto* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Attacker);
		auto* TargetASC = IsValid(Victim) ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Victim) : nullptr;
		if (!SourceASC) continue;

		const auto EffectContext = SourceASC->MakeEffectContext();

		for (const auto& Delta : Entry.Deltas)
		{
			if (!Delta.bIsPredictable || !Delta.EffectClass) continue;

			auto* ApplyTargetASC = Delta.bSelfTarget ? SourceASC : TargetASC;
			if (!ApplyTargetASC) continue;

			// Reconstruct a matching FCombatHitEffect so this goes through the exact same
			// construction UBaseCombatAbility::ApplySingleHitEffect (and therefore
			// ApplyHitEffects) uses — one application path, multiple callers, zero drift.
			// Delta.Amount is already the final signed predicted value (Amount * SignMultiplier
			// at level 1, per WolfPresageComponent::ResolveSimulatedImpact), so wrapping it in a
			// flat FScalableFloat and applying at level 1 reproduces it exactly.
			FCombatHitEffect ReAppliedEffect;
			ReAppliedEffect.EffectClass = Delta.EffectClass;
			ReAppliedEffect.bSelfTarget = Delta.bSelfTarget;
			ReAppliedEffect.Amount = FScalableFloat(Delta.Amount);

			const bool bApplied = UBaseCombatAbility::ApplySingleHitEffect(
				ReAppliedEffect, SourceASC, ApplyTargetASC, EffectContext, 1.f);

			if (!bApplied) continue;

			// Divergence guard: compare the real post-application value against the corresponding
			// buffer frame's predicted value at Entry.ImpactTime. Per stage 1's
			// magnitude-predictability rule this should never fire; this is a log, not a
			// correction — do not "fix up" real state to match prediction silently.
			AActor* PredictedStateActor = Delta.bSelfTarget ? Attacker : Victim;
			const auto* PredictedCombatant = Cast<IWolfCombatant>(PredictedStateActor);
			const auto* PredictedPresage = PredictedCombatant ? PredictedCombatant->GetPresageComponent() : nullptr;
			const auto* PredictedFrame = PredictedPresage ? PredictedPresage->GetSnapshotAtTime(Entry.ImpactTime) : nullptr;
			const auto* PredictedCharacter = Cast<AWolfCharacterBase>(PredictedStateActor);
			auto* PredictedAbilityControl = PredictedCharacter ? PredictedCharacter->FindComponentByClass<UWolfAbilityComponent>() : nullptr;

			if (PredictedFrame && PredictedAbilityControl)
			{
				const auto& CachedAttributes = PredictedAbilityControl->GetCachedAttributes();
				const int32 AttrIndex = CachedAttributes.IndexOfByPredicate(
					[&Delta](const FGameplayAttribute& Attr) { return Attr == Delta.Attribute; });

				if (CachedAttributes.IsValidIndex(AttrIndex) && PredictedFrame->AttributeValues.IsValidIndex(AttrIndex))
				{
					const float RealValue = ApplyTargetASC->GetNumericAttribute(Delta.Attribute);
					const float PredictedValue = PredictedFrame->AttributeValues[AttrIndex];

					if (!FMath::IsNearlyEqual(RealValue, PredictedValue, 0.5f))
					{
						WOLF_ERROR(TEXT("[PRESAGE] Divergence: %s real=%.2f predicted=%.2f for effect %s on %s at t=%.2fs"),
							*Delta.Attribute.GetName(), RealValue, PredictedValue,
							*Delta.EffectClass->GetName(),
							*PredictedStateActor->GetName(), Entry.ImpactTime);
					}
				}
			}
		}

		// ResourceLoop stage 1: apply the bake's RECORDED resource gains to the player through the
		// same live GE path (ApplyResourceGainToPlayer). Execution applies the exact values the
		// bake recorded (which the preview showed numerically) — exactness by construction, the
		// same rule as the damage deltas above.
		if (!FMath::IsNearlyZero(Entry.FlowGain) || !FMath::IsNearlyZero(Entry.AdrenalineGain))
		{
			ApplyResourceGainToPlayer(Entry.FlowGain, Entry.AdrenalineGain);
		}
	}
}

bool UCombatModeSubsystem::CheckExecutionDamageExit(float InExecutionClock, TArray<TWeakObjectPtr<AActor>>& OutVictims)
{
	// BakeImpactLedger is time-ordered (append order during the bake is already time-ordered), so
	// DamageExitCursor only ever advances — no rescanning from zero every tick.
	float MatchedImpactTime = -1.f;

	while (DamageExitCursor < BakeImpactLedger.Num())
	{
		const FPresageImpactEntry& Entry = BakeImpactLedger[DamageExitCursor];
		if (Entry.ImpactTime > InExecutionClock) break; // not due yet

		// Stop at the first qualifying impact time — don't consume entries past it. Multiple
		// victims hit at the exact same ImpactTime (e.g. an AoE) are collected together; a later,
		// separate impact time is left for a future tick to discover.
		if (MatchedImpactTime >= 0.f && !FMath::IsNearlyEqual(Entry.ImpactTime, MatchedImpactTime))
		{
			break;
		}

		++DamageExitCursor;

		if (Entry.bConnected && Entry.bVictimIsPlayerOrLinked)
		{
			MatchedImpactTime = Entry.ImpactTime;
			OutVictims.Add(Entry.Victim);
		}
	}

	return OutVictims.Num() > 0;
}

bool UCombatModeSubsystem::CheckEndingAction(float InExecutionClock) const
{
	const auto* Settings = GetDefault<UWolfCombatSettings>();
	if (!Settings || !Settings->TBEndingActionArchetypeTag.IsValid()) return false;

	for (const auto& Combatant : TrackedCombatants)
	{
		auto* Presage = Combatant.GetInterface() ? Combatant.GetInterface()->GetPresageComponent() : nullptr;
		if (!IsValid(Presage)) continue;

		const auto* Frame = Presage->GetSnapshotAtTime(InExecutionClock);
		if (!Frame || !Frame->ActiveAbility.IsValid()) continue;

		if (Frame->ActiveAbility->ArchetypeTag == Settings->TBEndingActionArchetypeTag)
		{
			const auto* CombatantActor = Cast<AActor>(Combatant.GetObject());
			WOLF_LOG(Log, TEXT("[PRESAGE] Ending action recognized: %s on %s at t=%.2fs (bonus not yet implemented)."),
				*Frame->ActiveAbility->GetName(),
				CombatantActor ? *CombatantActor->GetName() : TEXT("Unknown"),
				InExecutionClock);
			return true;
		}
	}
	return false;
}

void UCombatModeSubsystem::ScrubTimeline(float NewTime)
{
	const auto ClampedTime = FMath::Clamp(NewTime, 0.f, MaxTimelineDuration);
	if (FMath::IsNearlyEqual(ClampedTime, CurrentTimelineTime)) return; // No time change; no snapshot change
	CurrentTimelineTime = ClampedTime;

	// Presentational during Executing (real GE application is the source of truth for attributes
	// then); Full everywhere else (this IS the Planning-phase preview). See ERestoreDetail.
	const auto DetailForThisScrub = TBPhase == ETBPhase::Executing
		? ERestoreDetail::Presentational
		: ERestoreDetail::Full;

	for (auto& Combatant : TrackedCombatants)
	{
		auto* Obj = Combatant.GetObject();
		if (!IsValid(Obj)) continue;

		const auto* Presage = Combatant.GetInterface()->GetPresageComponent();
		if (!Presage) continue;

		if (auto* Character = Cast<AWolfCharacterBase>(Obj))
		{
			if (auto* SnapshotComp = Character->GetSnapshotComponent())
			{
				SnapshotComp->SetRestoreDetail(DetailForThisScrub);
			}
		}

		const auto* BakedFrame = Presage->GetSnapshotAtTime(CurrentTimelineTime);
		if (BakedFrame) ISnapshot::Execute_RestoreSnapshot(Obj, *BakedFrame);
		else WOLF_WARN(TEXT("No snapshot found for %s at %.2fs"), *Obj->GetName(), CurrentTimelineTime);
	}

	WOLF_LOG(Log, TEXT("Timeline Scrubbed to : %.2fs /  %.2fs"), CurrentTimelineTime, MaxTimelineDuration);
}

void UCombatModeSubsystem::ReBakeTimeline()
{
	if (!bIsInTB) return;

	const auto* World = GetWorld();
	if (!IsValid(World)) return;

	for (auto& Combatant : TrackedCombatants)
	{
		auto* Obj = Combatant.GetObject();
		if (!IsValid(Obj)) continue;

		const auto* AnchorState = MasterStartSnapshot.ActorStates.Find(Cast<AActor>(Obj));
		if (AnchorState)
		{
			ISnapshot::Execute_RestoreSnapshot(Obj, *AnchorState);
		}
	}

	const float ScrubTime = CurrentTimelineTime;

	// Re-initialize to the SAME session seed (not a new random one) — this is the load-bearing
	// subtlety: resetting to PresageSessionSeed means identical inputs re-roll identically on
	// this re-bake; a stream that merely persisted across re-bakes would still diverge because it
	// would have already consumed values from the previous pass. See PresageDeterminism.md.
	PresageRandomStream.Initialize(PresageSessionSeed);

	auto Plan = FPresageOrchestrator::RunPlanning(this, TrackedCombatants, MaxTimelineDuration);
	FPresageOrchestrator::ResolveInterrupts(Plan, PresageRandomStream);
	FPresageOrchestrator::DistributePlan(TrackedCombatants, Plan);

	BakeImpactLedger.Empty();
	DamageExitCursor = 0;
	LedgerApplicationCursor = 0;
	FWolfPresageSimulator::ExecuteFutureBake(TrackedCombatants, MaxTimelineDuration, BakedStepSize);

	CurrentTimelineTime = -1.f;
	ScrubTimeline(ScrubTime);

	WOLF_LOG(Log, TEXT("Timeline re-baked with injected abilities. Scrub position: %.2fs"), ScrubTime);
}

void UCombatModeSubsystem::HandlePresageDrainEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode)
{
	FWolfPresageSimulator::ApplyPresageDrainEffect(ASC, CurrentActorMode, PresageEffectClass);
}

void UCombatModeSubsystem::ApplyModeToActor(const TScriptInterface<IWolfCombatant>& Combatant, FGameplayTag NewMode)
{
	auto* Obj = Combatant.GetObject();
	auto* Actor = Cast<AActor>(Obj);
	if (!IsValid(Actor)) return;

	auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	if (!ASC) return;

	// Enemies and unlinked allies don't need to switch.
	const auto* Pawn = Cast<APawn>(Actor);
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
	WOLF_LOG(Log, TEXT("Adding Mode Tag %s for %s"), *ActualModeForActor.ToString(), *Actor->GetName());

	if (bIsPlayer)
	{
		// Drain GE retired (PresagePreviewStage3): the periodic drain never ticked at dilation 0
		// (verified code fact) and is conceptually vestigial under the budget model — TB duration is
		// granted at entry and consumed by the execution clock, not drained by a live effect.
		// HandlePresageDrainEffect / FWolfPresageSimulator::ApplyPresageDrainEffect / UPresageMode /
		// PresageEffectClass are left in place (deprecated) until ResourceLoop stage 3 confirms
		// nothing else needs the seam — see HandlePresageDrainEffect's declaration.
	}

	// Safest way to call UINTERFACE functions that might be implemented in C++ or BP.
	if (Combatant.GetInterface())
	{
		IWolfCombatant::Execute_OnCombatModeChanged(Combatant.GetObject(), ActualModeForActor);
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

const FAbilityTimingProfile& UCombatModeSubsystem::GetOrComputeTimingProfile(const TSubclassOf<UBaseCombatAbility>& AbilityClass)
{
	static constexpr FAbilityTimingProfile DefaultProfile;
	if (!AbilityClass) return DefaultProfile;
	if (const FAbilityTimingProfile* Existing = TimingProfileCache.Find(AbilityClass)) return *Existing;

	const auto* CDO = AbilityClass->GetDefaultObject<UBaseCombatAbility>();
	const FAbilityTimingProfile Computed = CDO
		? UBaseCombatAbility::ComputeAbilityTiming(CDO->GetAbilitySequence(), CDO->HitEventTag)
		: FAbilityTimingProfile();

	return TimingProfileCache.Add(AbilityClass, Computed);
}

void UCombatModeSubsystem::ApplyResourceGainToPlayer(float FlowDelta, float AdrenalineDelta)
{
	EnsureResourceTelemetryBound();

	if (FMath::IsNearlyZero(FlowDelta) && FMath::IsNearlyZero(AdrenalineDelta)) return;

	UAbilitySystemComponent* PlayerASC = GetPlayerASC();
	if (!PlayerASC)
	{
		WOLF_WARN(TEXT("[RESOURCES] Cannot apply resource gain (Flow %f, Adrenaline %f) — no player ASC yet."),
			FlowDelta, AdrenalineDelta);
		return;
	}

	const auto SpecHandle = PlayerASC->MakeOutgoingSpec(
		UResourceGainEffect::StaticClass(), 1.f, PlayerASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		WOLF_WARN(TEXT("[RESOURCES] Failed to create UResourceGainEffect spec — gain not applied."));
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(FWolfGameplayTags::Get().Data_FlowAmount, FlowDelta);
	SpecHandle.Data->SetSetByCallerMagnitude(FWolfGameplayTags::Get().Data_AdrenalineAmount, AdrenalineDelta);
	PlayerASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UCombatModeSubsystem::EnsureResourceTelemetryBound()
{
	if (bResourceTelemetryBound) return;

	UAbilitySystemComponent* PlayerASC = GetPlayerASC();
	if (!PlayerASC) return;

	PlayerASC->GetGameplayAttributeValueChangeDelegate(UWolfAttributeSet::GetFlowGaugeAttribute())
		.AddUObject(this, &UCombatModeSubsystem::OnPlayerFlowGaugeChanged);
	PlayerASC->GetGameplayAttributeValueChangeDelegate(UWolfAttributeSet::GetAdrenalineAttribute())
		.AddUObject(this, &UCombatModeSubsystem::OnPlayerAdrenalineChanged);

	bResourceTelemetryBound = true;
	WOLF_LOG(Log, TEXT("[RESOURCES] Player resource telemetry bound (Flow Gauge / Adrenaline)."));
}

void UCombatModeSubsystem::OnPlayerFlowGaugeChanged(const FOnAttributeChangeData& Data)
{
	const float Delta = Data.NewValue - Data.OldValue;
	OnFlowGaugeChanged.Broadcast(Data.NewValue, Delta);

#if WOLF_DEBUG_ENABLED
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(101, 3.f, FColor::Cyan,
			FString::Printf(TEXT("Flow Gauge: %.1f (%+.1f)"), Data.NewValue, Delta));
	}
#endif
}

void UCombatModeSubsystem::OnPlayerAdrenalineChanged(const FOnAttributeChangeData& Data)
{
	const float Delta = Data.NewValue - Data.OldValue;
	OnAdrenalineChanged.Broadcast(Data.NewValue, Delta);

#if WOLF_DEBUG_ENABLED
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(102, 3.f, FColor::Yellow,
			FString::Printf(TEXT("Adrenaline: %.1f (%+.1f)"), Data.NewValue, Delta));
	}
#endif
}