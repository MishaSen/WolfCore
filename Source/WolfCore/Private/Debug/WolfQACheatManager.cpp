// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/WolfQACheatManager.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/WolfAttributeSet.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/WolfCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Core/WolfCombatSettings.h"
#include "Core/WolfFunctionLibrary.h"
#include "Core/WolfGameplayTags.h"
#include "Core/WolfPresageComponent.h"
#include "Core/WolfResourceRules.h"
#include "Debug/WolfDebug.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagsManager.h"
#include "Math/RotationMatrix.h"
#include "Presage/PresageOrchestratorTypes.h"
#include "Systems/CombatModeSubsystem.h"

/** Default placement distance (cm) for the QA scenario commands. Chosen to sit inside the default
  * authored attack Range (FCombatPeriod::Range defaults to 150 cm; URTCombatAbility::AttackRange
  * to 150 cm) so both combatants are within each other's range after teleporting, while leaving
  * clearance for capsule overlap. Abilities whose Range is authored differently in-editor may need
  * a larger/smaller value — this is documented best-effort, per the implementation spec. */
static constexpr float QAScenarioSpacing = 120.f;

// ============================================================================================================================
// Private Helpers
// ============================================================================================================================

UCombatModeSubsystem* UWolfQACheatManager::GetCMS() const
{
	return UWolfFunctionLibrary::GetWorldSubsystem<UCombatModeSubsystem>(this);
}

UAbilitySystemComponent* UWolfQACheatManager::GetTargetASC(AActor* Target) const
{
	if (!Target) return nullptr;
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
}

AActor* UWolfQACheatManager::ResolveQATarget(const FString& TargetName) const
{
	auto* CMS = GetCMS();
	if (!CMS) return nullptr;

	// Empty name resolves to the local player's pawn (standard for UCheatManager subclasses).
	if (TargetName.IsEmpty())
	{
		const auto* PC = GetOuterAPlayerController();
		return PC ? PC->GetPawn() : nullptr;
	}

	// Non-empty: case-insensitive substring match against tracked combatant actor names.
	TArray<AActor*> Matches;
	for (const auto& Combatant : CMS->GetTrackedCombatants())
	{
		auto* CombatantActor = Cast<AActor>(Combatant.GetObject());
		if (!CombatantActor) continue;

		if (CombatantActor->GetName().Contains(TargetName, ESearchCase::IgnoreCase))
		{
			Matches.Add(CombatantActor);
		}
	}

	if (Matches.Num() > 1)
	{
		WOLF_WARN(TEXT("[QA] Target \"%s\" matches %d tracked combatants (%s, %s, ...) — using first match."),
			*TargetName, Matches.Num(), *Matches[0]->GetName(), *Matches[1]->GetName());
	}

	if (Matches.Num() == 0)
	{
		WOLF_WARN(TEXT("[QA] Target \"%s\" matched no tracked combatant."), *TargetName);
		return nullptr;
	}

	return Matches[0];
}

void UWolfQACheatManager::RestoreCombatant(AActor* Target) const
{
	auto* Character = Cast<AWolfCharacterBase>(Target);
	if (!Character)
	{
		WOLF_LOG(Log, TEXT("[QA] RestoreCombatant: target is not a WolfCharacterBase — nothing to restore."));
		return;
	}

	auto* ASC = GetTargetASC(Character);
	if (!ASC)
	{
		WOLF_LOG(Log, TEXT("[QA] RestoreCombatant: %s has no ASC — nothing to restore."), *Character->GetName());
		return;
	}

	// Re-enable the capsule exactly as RestorePhysics does (Die() disabled it and set all
	// channel responses to ignore; the profile reset restores the authored responses).
	if (auto* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	}

	// Re-enable character movement (Die() called DisableMovement()). Keeping the mode walk is a
	// best-effort default for a QA restore; a setup that needs a different resting mode can re-set
	// it after the restore.
	if (auto* MoveComp = Character->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	// Clear the dead tag exactly as RestoreGAS's Full-restore path does.
	ASC->SetTagMapCount(FWolfGameplayTags::Get().InputState_Dead, 0);

	// Restore Health to MaxHealth via the same non-lethal ApplyModToAttribute poke QA_SetHealth
	// uses (runs PreAttributeChange's clamp, does not fire PostGameplayEffectExecute / Die()).
	const float MaxHealth = ASC->GetNumericAttribute(UWolfAttributeSet::GetMaxHealthAttribute());
	ASC->ApplyModToAttribute(UWolfAttributeSet::GetHealthAttribute(), EGameplayModOp::Override, MaxHealth);

	WOLF_INFO(TEXT("[QA] Restored %s (capsule, movement, dead tag cleared, health -> %.1f)."),
		*Character->GetName(), MaxHealth);
}

void UWolfQACheatManager::ClearCombatantTarget(AActor* Target) const
{
	if (!Target) return;

	auto* TargetPawn = Cast<APawn>(Target);
	auto* AIController = TargetPawn ? Cast<AAIController>(TargetPawn->GetController()) : nullptr;
	if (!AIController || !AIController->GetBlackboardComponent())
	{
		WOLF_WARN(TEXT("[QA] %s is not an AI-possessed combatant with a blackboard — cannot clear its target."),
			*Target->GetName());
		return;
	}

	AIController->GetBlackboardComponent()->SetValueAsObject(TEXT("TargetActor"), nullptr);
	WOLF_INFO(TEXT("[QA] Cleared %s's blackboard TargetActor."), *Target->GetName());
}

// ============================================================================================================================
// Layer 1 — State Injection
// ============================================================================================================================

void UWolfQACheatManager::QA_SetFlow(float Value)
{
	auto* CMS = GetCMS();
	if (!CMS) return;

	auto* PlayerASC = CMS->GetPlayerASC();
	if (!PlayerASC)
	{
		WOLF_WARN(TEXT("[QA] QA_SetFlow: no player ASC yet — nothing to set."));
		return;
	}

	const float Current = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetFlowGaugeAttribute());
	const float Delta = Value - Current;

	// Real GE pipeline: PreAttributeChange clamps (soft cap, floor) and PostGameplayEffectExecute
	// logging fire exactly as they would for a real hit. Setting above MaxFlowGauge is a valid test
	// of the soft-cap clamp (test 33).
	CMS->ApplyResourceGainToPlayer(Delta, 0.f);

	WOLF_INFO(TEXT("[QA] QA_SetFlow: current %.2f -> requested %.2f (delta %.2f applied via gain path)."),
		Current, Value, Delta);
}

void UWolfQACheatManager::QA_SetAdrenaline(float Value)
{
	auto* CMS = GetCMS();
	if (!CMS) return;

	auto* PlayerASC = CMS->GetPlayerASC();
	if (!PlayerASC)
	{
		WOLF_WARN(TEXT("[QA] QA_SetAdrenaline: no player ASC yet — nothing to set."));
		return;
	}

	const float Current = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetAdrenalineAttribute());
	const float Delta = Value - Current;

	CMS->ApplyResourceGainToPlayer(0.f, Delta);

	WOLF_INFO(TEXT("[QA] QA_SetAdrenaline: current %.2f -> requested %.2f (delta %.2f applied via gain path)."),
		Current, Value, Delta);
}

void UWolfQACheatManager::QA_SetHealth(float Value, FString TargetName)
{
	AActor* Target = ResolveQATarget(TargetName);
	auto* ASC = GetTargetASC(Target);
	if (!Target || !ASC)
	{
		WOLF_WARN(TEXT("[QA] QA_SetHealth: resolved target%s has no ASC."), Target ? *Target->GetName() : TEXT("(none)"));
		return;
	}

	if (Value <= 0.f)
	{
		WOLF_WARN(TEXT("[QA] QA_SetHealth: %s set to %.1f is a NON-LETHAL poke — ApplyModToAttribute runs PreAttributeChange's clamp but not PostGameplayEffectExecute, so Die() will NOT fire. Use QA_KillTarget for real lethal setup."),
			*Target->GetName(), Value);
	}

	ASC->ApplyModToAttribute(UWolfAttributeSet::GetHealthAttribute(), EGameplayModOp::Override, Value);
	WOLF_INFO(TEXT("[QA] QA_SetHealth: %s health -> %.1f."), *Target->GetName(), Value);
}

void UWolfQACheatManager::QA_KillTarget(FString TargetName)
{
	AActor* Target = ResolveQATarget(TargetName);
	auto* Character = Cast<AWolfCharacterBase>(Target);
	if (!Character)
	{
		WOLF_WARN(TEXT("[QA] QA_KillTarget: resolved target%s is not a WolfCharacterBase."), Target ? *Target->GetName() : TEXT("(none)"));
		return;
	}

	// The exact same call PostGameplayEffectExecute makes on a real lethal hit — real death side
	// effects (collision/movement disable, InputState.Dead tag, "has died" message) all fire.
	Character->Die();
}

void UWolfQACheatManager::QA_RestoreTarget(FString TargetName)
{
	RestoreCombatant(ResolveQATarget(TargetName));
}

void UWolfQACheatManager::QA_ClearTarget(FString TargetName)
{
	ClearCombatantTarget(ResolveQATarget(TargetName));
}

void UWolfQACheatManager::QA_SelectTarget(FString AIName, FString TargetName)
{
	AActor* AIPawn = ResolveQATarget(AIName);
	AActor* Target = ResolveQATarget(TargetName);
	if (!AIPawn || !Target)
	{
		WOLF_WARN(TEXT("[QA] QA_SelectTarget: could not resolve AI (%s) and/or target (%s)."),
			*AIName, *TargetName);
		return;
	}

	auto* AIPawnActual = Cast<APawn>(AIPawn);
	auto* AIController = AIPawnActual ? Cast<AAIController>(AIPawnActual->GetController()) : nullptr;
	if (!AIController || !AIController->GetBlackboardComponent())
	{
		WOLF_WARN(TEXT("[QA] QA_SelectTarget: %s is not an AI-possessed combatant with a blackboard."),
			*AIPawn->GetName());
		return;
	}

	AIController->GetBlackboardComponent()->SetValueAsObject(TEXT("TargetActor"), Target);
	WOLF_INFO(TEXT("[QA] QA_SelectTarget: %s -> %s."), *AIPawn->GetName(), *Target->GetName());
}

void UWolfQACheatManager::QA_ResetCombat()
{
	auto* CMS = GetCMS();
	if (!CMS) return;

	// Zero the player's Flow and Adrenaline via the same ApplyResourceGainToPlayer delta pattern the
	// setters use — same clamps/logging as real gains, so resetting can't leave un-clamped state.
	if (auto* PlayerASC = CMS->GetPlayerASC())
	{
		const float Flow = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetFlowGaugeAttribute());
		const float Adrenaline = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetAdrenalineAttribute());
		CMS->ApplyResourceGainToPlayer(-Flow, -Adrenaline);
	}

	// Restore every tracked combatant from any live death state.
	for (const auto& Combatant : CMS->GetTrackedCombatants())
	{
		RestoreCombatant(Cast<AActor>(Combatant.GetObject()));
	}

	// Abandon TB if active so every test starts from a known RT baseline. SetMode's RT branch
	// handles the Planning/abandon scrubbing internally.
	if (CMS->GetCurrentMode() == FWolfGameplayTags::Get().InputState_TB)
	{
		CMS->SetMode(FWolfGameplayTags::Get().InputState_RT);
	}

	WOLF_INFO(TEXT("[QA] QA_ResetCombat: Flow/Adrenaline zeroed, all combatants restored, mode -> RT."));
}

void UWolfQACheatManager::QA_ActivateAbility(FString TargetName, FString InputTagName)
{
	auto* Target = ResolveQATarget(TargetName);
	auto* ASC = Cast<UWolfAbilitySystemComponent>(GetTargetASC(Target));
	const auto Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*InputTagName), false);
	if (!ASC || !Tag.IsValid())
	{
		WOLF_WARN(TEXT("[QA} QA_ActivateAbility: bad target (%s) or tag (%s)."), *TargetName, *InputTagName);
		return;
	}
	ASC->AbilityInputTagPressed(Tag);
}

// ============================================================================================================================
// Layer 2 — Scenario Construction
// ============================================================================================================================

void UWolfQACheatManager::QA_Scenario_Interrupt(FString AttackerName, FString VictimName)
{
	AActor* Attacker = ResolveQATarget(AttackerName);
	AActor* Victim = ResolveQATarget(VictimName);
	if (!Attacker || !Victim)
	{
		WOLF_WARN(TEXT("[QA] QA_Scenario_Interrupt: could not resolve attacker (%s) and/or victim (%s)."),
			*AttackerName, *VictimName);
		return;
	}

	// Unqueue both so FPresageOrchestrator's stub-target resolution (first other tracked combatant)
	// picks each other — that is what actually makes the interrupt scenario take shape.
	ClearCombatantTarget(Attacker);
	ClearCombatantTarget(Victim);

	const FVector Direction = (Victim->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
	const FVector PlacementDirection = Direction.IsNearlyZero()
		? FVector::ForwardVector
		: Direction;

	// Facing each other, close enough that both are in each other's attack range (best-effort —
	// whether the ability windows actually overlap on the baked timeline still depends on intent
	// selection and disposition rolls).
	Attacker->SetActorLocationAndRotation(
		Attacker->GetActorLocation(),
		PlacementDirection.Rotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	Victim->SetActorLocationAndRotation(
		Attacker->GetActorLocation() + PlacementDirection * QAScenarioSpacing,
		(-PlacementDirection).Rotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	WOLF_INFO(TEXT("[QA] QA_Scenario_Interrupt: %s and %s placed %.0f cm apart, facing each other, targets cleared."),
		*Attacker->GetName(), *Victim->GetName(), QAScenarioSpacing);
}

void UWolfQACheatManager::QA_Scenario_TBDamageExit(FString AttackerName)
{
	const auto* PC = GetOuterAPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	if (!PlayerPawn)
	{
		WOLF_WARN(TEXT("[QA] QA_Scenario_TBDamageExit: no player pawn."));
		return;
	}

	AActor* Attacker = ResolveQATarget(AttackerName);
	if (!Attacker)
	{
		WOLF_WARN(TEXT("[QA] QA_Scenario_TBDamageExit: could not resolve attacker \"%s\"."), *AttackerName);
		return;
	}

	const FVector Direction = (PlayerPawn->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
	const FVector PlacementDirection = Direction.IsNearlyZero()
		? FVector::ForwardVector
		: Direction;

	// Within the default authored attack Range (150 cm) of the player, facing the player. If the
	// attacker is the only non-player tracked combatant, stub-target resolution deterministically
	// picks the player — making test 14 reliable instead of trial-and-error.
	Attacker->SetActorLocationAndRotation(
		PlayerPawn->GetActorLocation() - PlacementDirection * QAScenarioSpacing,
		PlacementDirection.Rotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	WOLF_INFO(TEXT("[QA] QA_Scenario_TBDamageExit: %s placed %.0f cm from player, facing player."),
		*Attacker->GetName(), QAScenarioSpacing);
}

void UWolfQACheatManager::QA_Scenario_AirborneExtension(FString AttackerName, FString VictimName)
{
	AActor* Attacker = ResolveQATarget(AttackerName);
	AActor* Victim = ResolveQATarget(VictimName);
	if (!Attacker || !Victim)
	{
		WOLF_WARN(TEXT("[QA] QA_Scenario_AirborneExtension: could not resolve attacker (%s) and/or victim (%s)."),
			*AttackerName, *VictimName);
		return;
	}

	const FVector Direction = (Victim->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
	const FVector PlacementDirection = Direction.IsNearlyZero()
		? FVector::ForwardVector
		: Direction;

	// Same placement pattern as the interrupt scenario — close enough that both the launch ability
	// and the follow-up extension ability can land on the same victim without repositioning between
	// them (AttackerName is typically empty -> the player).
	Attacker->SetActorLocationAndRotation(
		Attacker->GetActorLocation(),
		PlacementDirection.Rotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	Victim->SetActorLocationAndRotation(
		Attacker->GetActorLocation() + PlacementDirection * QAScenarioSpacing,
		(-PlacementDirection).Rotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	WOLF_INFO(TEXT("[QA] QA_Scenario_AirborneExtension: %s and %s placed %.0f cm apart, facing each other."),
		*Attacker->GetName(), *Victim->GetName(), QAScenarioSpacing);
}

void UWolfQACheatManager::QA_Scenario_LinkedEnemies(int32 Count, float Spacing)
{
	auto* CMS = GetCMS();
	if (!CMS) return;

	const auto* PC = GetOuterAPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	if (!PlayerPawn)
	{
		WOLF_WARN(TEXT("[QA] QA_Scenario_LinkedEnemies: no player pawn."));
		return;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const FVector Forward = PlayerPawn->GetActorForwardVector();
	// Facing away from the player: their forward vector points opposite the player's forward.
	const FRotator AwayRotation = FRotationMatrix::MakeFromX(Forward).Rotator();

	int32 Moved = 0;
	for (const auto& Combatant : CMS->GetTrackedCombatants())
	{
		if (Moved >= Count) break;

		auto* CombatantActor = Cast<AActor>(Combatant.GetObject());
		if (!CombatantActor) continue;

		const auto* CombatantPawn = Cast<APawn>(CombatantActor);
		if (CombatantPawn && CombatantPawn->IsPlayerControlled()) continue;

		CombatantActor->SetActorLocationAndRotation(
			PlayerLocation + Forward * (Spacing * static_cast<float>(Moved + 1)),
			AwayRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		++Moved;
	}

	WOLF_INFO(TEXT("[QA] QA_Scenario_LinkedEnemies: lined up %d enemy(ies) at %.0f cm spacing in front of player."),
		Moved, Spacing);
}

// ============================================================================================================================
// Layer 3 — Observability
// ============================================================================================================================

void UWolfQACheatManager::QA_DumpCombatState()
{
	auto* CMS = GetCMS();
	if (!CMS) return;

	const FWolfGameplayTags& Tags = FWolfGameplayTags::Get();

	WOLF_LOG(Log, TEXT("[QA] === Combat State Dump ==="));
	WOLF_LOG(Log, TEXT("[QA] Mode: %s | TBPhase: %s | TimelineTime: %.2fs | BudgetedTBDuration: %.2fs"),
		*CMS->GetCurrentMode().ToString(),
		*UEnum::GetValueAsString(CMS->GetTBPhase()),
		CMS->GetCurrentTimelineTime(),
		CMS->GetBudgetedTBDuration());

	if (auto* PlayerASC = CMS->GetPlayerASC())
	{
		const float Flow = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetFlowGaugeAttribute());
		const float MaxFlow = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetMaxFlowGaugeAttribute());
		const float Interval = GetDefault<UWolfCombatSettings>()->FlowThresholdInterval;
		const int32 Stage = FWolfResourceRules::GetThresholdStage(Flow, MaxFlow, Interval);
		const float Adrenaline = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetAdrenalineAttribute());
		const float Health = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetHealthAttribute());

		WOLF_LOG(Log, TEXT("[QA] Player: Flow=%.2f MaxFlow=%.2f Stage=%d Adrenaline=%.2f Health=%.2f"),
			Flow, MaxFlow, Stage, Adrenaline, Health);
	}
	else
	{
		WOLF_LOG(Log, TEXT("[QA] Player: <no ASC>"));
	}

	// One line per tracked non-player combatant.
	for (const auto& Combatant : CMS->GetTrackedCombatants())
	{
		auto* CombatantActor = Cast<AActor>(Combatant.GetObject());
		if (!CombatantActor) continue;

		const auto* CombatantPawn = Cast<APawn>(CombatantActor);
		if (CombatantPawn && CombatantPawn->IsPlayerControlled()) continue;

		auto* CombatantASC = GetTargetASC(CombatantActor);
		const float Health = CombatantASC
			? CombatantASC->GetNumericAttribute(UWolfAttributeSet::GetHealthAttribute())
			: -1.f;

		FString TagBit;
		if (CombatantASC)
		{
			TagBit = FString::Printf(TEXT("RT=%d TB=%d Dead=%d Link=%d Airborne=%d"),
				CombatantASC->HasMatchingGameplayTag(Tags.InputState_RT) ? 1 : 0,
				CombatantASC->HasMatchingGameplayTag(Tags.InputState_TB) ? 1 : 0,
				CombatantASC->HasMatchingGameplayTag(Tags.InputState_Dead) ? 1 : 0,
				CombatantASC->HasMatchingGameplayTag(Tags.Status_Link) ? 1 : 0,
				CombatantASC->HasMatchingGameplayTag(Tags.Status_Airborne) ? 1 : 0);
		}
		else
		{
			TagBit = TEXT("<no ASC>");
		}

		WOLF_LOG(Log, TEXT("[QA] Combatant %s: Health=%.2f %s"), *CombatantActor->GetName(), Health, *TagBit);
	}
}

void UWolfQACheatManager::QA_DumpResources()
{
	auto* CMS = GetCMS();
	if (!CMS) return;

	auto* PlayerASC = CMS->GetPlayerASC();
	if (!PlayerASC)
	{
		WOLF_WARN(TEXT("[QA] QA_DumpResources: no player ASC yet."));
		return;
	}

	const float Flow = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetFlowGaugeAttribute());
	const float MaxFlow = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetMaxFlowGaugeAttribute());
	const float Interval = GetDefault<UWolfCombatSettings>()->FlowThresholdInterval;
	const int32 Stage = FWolfResourceRules::GetThresholdStage(Flow, MaxFlow, Interval);
	const float Adrenaline = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetAdrenalineAttribute());
	const float Scalar = FWolfResourceRules::GetAdrenalineScalar(Adrenaline);

	WOLF_INFO(TEXT("[QA] Resources: Flow=%.2f MaxFlow=%.2f Stage=%d Adrenaline=%.2f SpendScalar=%.2f"),
		Flow, MaxFlow, Stage, Adrenaline, Scalar);
}

void UWolfQACheatManager::QA_DumpTags(FString TargetName)
{
	AActor* Target = ResolveQATarget(TargetName);
	auto* ASC = GetTargetASC(Target);
	if (!Target || !ASC)
	{
		WOLF_WARN(TEXT("[QA] QA_DumpTags: resolved target%s has no ASC."), Target ? *Target->GetName() : TEXT("(none)"));
		return;
	}

	FGameplayTagContainer OwnedTags;
	ASC->GetOwnedGameplayTags(OwnedTags);

	WOLF_LOG(Log, TEXT("[QA] %s owned gameplay tags (%d):"), *Target->GetName(), OwnedTags.Num());
	for (const FGameplayTag& Tag : OwnedTags)
	{
		WOLF_LOG(Log, TEXT("[QA]   %s"), *Tag.ToString());
	}
}

void UWolfQACheatManager::QA_DumpEffects(FString TargetName)
{
	AActor* Target = ResolveQATarget(TargetName);
	auto* ASC = GetTargetASC(Target);
	if (!Target || !ASC)
	{
		WOLF_WARN(TEXT("[QA] QA_DumpEffects: resolved target%s has no ASC."), Target ? *Target->GetName() : TEXT("(none)"));
		return;
	}

	const TArray<FActiveGameplayEffectHandle> ActiveEffects = ASC->GetActiveEffects(FGameplayEffectQuery());

	WOLF_LOG(Log, TEXT("[QA] %s active effects (%d):"), *Target->GetName(), ActiveEffects.Num());
	for (const FActiveGameplayEffectHandle& Handle : ActiveEffects)
	{
		const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(Handle);
		if (!ActiveEffect) continue;

		const UGameplayEffect* Def = ActiveEffect->Spec.Def;
		const UWorld* World = GetWorld();
		const float Remaining = ActiveEffect->GetTimeRemaining(World ? World->GetTimeSeconds() : 0.f);

		WOLF_LOG(Log, TEXT("[QA]   %s — remaining %.2fs"),
			Def ? *Def->GetClass()->GetName() : TEXT("Unknown"),
			Remaining);
	}
}

void UWolfQACheatManager::QA_DumpPlan()
{
	auto* CMS = GetCMS();
	if (!CMS) return;

	for (const auto& Combatant : CMS->GetTrackedCombatants())
	{
		auto* CombatantActor = Cast<AActor>(Combatant.GetObject());
		auto* Presage = Combatant ? Combatant->GetPresageComponent() : nullptr;
		if (!CombatantActor || !Presage) continue;

		const TArray<FIntentEntry>& Intents = Presage->GetPlannedIntents();

		WOLF_LOG(Log, TEXT("[QA] %s planned intents (%d):"), *CombatantActor->GetName(), Intents.Num());
		for (const FIntentEntry& Entry : Intents)
		{
			WOLF_LOG(Log, TEXT("[QA]   %s at t=%.2fs"),
				Entry.AbilityClass ? *Entry.AbilityClass->GetName() : TEXT("None"),
				Entry.StartTime);
		}
	}
}

void UWolfQACheatManager::QA_DumpImpactLedger()
{
	auto* CMS = GetCMS();
	if (!CMS) return;

	const TArray<FPresageImpactEntry>& Ledger = CMS->GetBakeImpactLedger();

	WOLF_LOG(Log, TEXT("[QA] Bake impact ledger (%d entries):"), Ledger.Num());
	for (const FPresageImpactEntry& Entry : Ledger)
	{
		const FString AttackerName = Entry.Attacker.IsValid() ? Entry.Attacker->GetName() : TEXT("None");
		const FString VictimName = Entry.Victim.IsValid() ? Entry.Victim->GetName() : TEXT("None");

		WOLF_LOG(Log, TEXT("[QA]   %s -> %s at t=%.2fs connected=%d FlowGain=%.2f AdrenalineGain=%.2f (%d deltas)"),
			*AttackerName, *VictimName, Entry.ImpactTime, Entry.bConnected ? 1 : 0,
			Entry.FlowGain, Entry.AdrenalineGain, Entry.Deltas.Num());

		for (const FPredictedEffectDelta& Delta : Entry.Deltas)
		{
			WOLF_LOG(Log, TEXT("[QA]     Delta: %s amount=%.2f predictable=%d"),
				*Delta.Attribute.GetName(), Delta.Amount, Delta.bIsPredictable ? 1 : 0);
		}
	}
}

void UWolfQACheatManager::QA_DumpTimeToNextHitImpact(FString TargetName)
{
	AActor* Target = ResolveQATarget(TargetName);
	auto* Character = Cast<AWolfCharacterBase>(Target);
	if (!Character)
	{
		WOLF_WARN(TEXT("[QA] QA_DumpTimeToNextHitImpact: resolved target%s is not a WolfCharacterBase."),
			Target ? *Target->GetName() : TEXT("(none)"));
		return;
	}

	const float TimeToNextImpact = Character->GetTimeToNextHitImpact();

	WOLF_INFO(TEXT("[QA] %s time to next hit impact: %.2fs%s"),
		*Character->GetName(),
		TimeToNextImpact,
		TimeToNextImpact < 0.f ? TEXT(" (no upcoming hit notify)") : TEXT(""));
}

// ============================================================================================================================
// Layer 4 — Assertions
// ============================================================================================================================

void UWolfQACheatManager::QA_AssertFloat(FString Label, float Actual, float Expected, float Tolerance)
{
	const bool bPass = FMath::IsNearlyEqual(Actual, Expected, Tolerance);

	if (bPass)
	{
		WOLF_INFO(TEXT("[QA][PASS] %s (expected=%.3f, actual=%.3f, tol=%.3f)"),
			*Label, Expected, Actual, Tolerance);
	}
	else
	{
		WOLF_ERROR(TEXT("[QA][FAIL] %s (expected=%.3f, actual=%.3f, tol=%.3f)"),
			*Label, Expected, Actual, Tolerance);
	}
}

void UWolfQACheatManager::QA_AssertTag(FString TargetName, FString TagName, bool bExpected)
{
	AActor* Target = ResolveQATarget(TargetName);
	auto* ASC = GetTargetASC(Target);
	if (!Target || !ASC)
	{
		WOLF_ERROR(TEXT("[QA][FAIL] %s: resolved target%s has no ASC."),
			*TagName, Target ? *Target->GetName() : TEXT("(none)"));
		return;
	}

	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName), false);
	if (!Tag.IsValid())
	{
		WOLF_ERROR(TEXT("[QA][FAIL] %s: tag name is not registered in the GameplayTagsManager."), *TagName);
		return;
	}

	const bool bHas = ASC->HasMatchingGameplayTag(Tag);
	const bool bPass = (bHas == bExpected);

	if (bPass)
	{
		WOLF_INFO(TEXT("[QA][PASS] %s on %s (expected=%s, actual=%s)"),
			*TagName, *Target->GetName(),
			bExpected ? TEXT("true") : TEXT("false"),
			bHas ? TEXT("true") : TEXT("false"));
	}
	else
	{
		WOLF_ERROR(TEXT("[QA][FAIL] %s on %s (expected=%s, actual=%s)"),
			*TagName, *Target->GetName(),
			bExpected ? TEXT("true") : TEXT("false"),
			bHas ? TEXT("true") : TEXT("false"));
	}
}

// ============================================================================================================================
// Layer 5 — TB Control
// ============================================================================================================================

void UWolfQACheatManager::QA_LockInTB()
{
	auto* CMS = GetCMS();
	if (!CMS) return;

	// LockInPlan() is already public C++ — this wrapper is the entire fix (nothing in the input
	// chain called it, and it wasn't console/reflection-reachable before).
	CMS->LockInPlan();
}

void UWolfQACheatManager::QA_ScrubTB(float Time)
{
	auto* CMS = GetCMS();
	if (!CMS) return;

	// Precise numeric scrub instead of an analog/mouse input — lets tests land on an exact frame.
	CMS->ScrubTimeline(Time);
}