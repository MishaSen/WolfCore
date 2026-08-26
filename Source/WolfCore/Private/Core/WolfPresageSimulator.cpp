// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCore/Public/Core/WolfPresageSimulator.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BaseCombatAbility.h"
#include "AIController.h"
#include "Core/WolfGameplayTags.h"
#include "Core/WolfPresageComponent.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/IWolfCombatant.h"
#include "Navigation/PathFollowingComponent.h"
#include "Presage/ActorSnapshot.h"

void FWolfPresageSimulator::ExecuteFutureBake(const TArray<TScriptInterface<IWolfCombatant>>& Combatants, float Duration, float StepSize)
{
	if (Combatants.Num() == 0 || FMath::IsNearlyEqual(Duration, 0.f)) return;

	// Phase 1: Setup each combatant's simulation state.
	for (auto& Combatant : Combatants)
	{
		SetupCombatantSimulation(Combatant, Duration);
	}

	// Capture the true t=0 frame for each combatant — one frame per combatant, no stepping yet.
	// Must happen after setup (so ClearPredictionBuffer has already run) and before the step loop.
	for (auto& Combatant : Combatants)
	{
		auto* Presage = Combatant.GetInterface() ? Combatant.GetInterface()->GetPresageComponent() : nullptr;
		if (IsValid(Presage))
		{
			Presage->CaptureCurrentFrame();
		}
	}

	// Phase 2: Bake simulation steps.
	const int32 TotalSteps = FMath::CeilToInt(Duration / StepSize);

	WOLF_LOG(Log, TEXT("Baking Future: %d steps over %.2fs"), TotalSteps, Duration);

	for (int32 i = 0; i < TotalSteps; ++i)
	{
		//WOLF_LOG(Log, TEXT("[SIM] Step %d"), i);
		for (auto& Combatant : Combatants)
		{
			BakeSimulationStep(Combatant, StepSize);
		}
	}

	// Phase 3: Cleanup each combatant's simulation state.
	for (auto& Combatant : Combatants)
	{
		CleanupCombatantSimulation(Combatant);
	}
}

void FWolfPresageSimulator::SetupCombatantSimulation(const TScriptInterface<IWolfCombatant>& Combatant, float Duration)
{
	const auto* Actor = Cast<AActor>(Combatant.GetObject());
	if (!IsValid(Actor)) return;

	auto* Presage = Combatant.GetInterface()->GetPresageComponent();
	if (!IsValid(Presage)) return;

	// Clear prediction buffer. SetSimPeriodTime to 0 so AdvancePeriod computes
	// remaining duration from the actor's current position, not elapsed time.
	Presage->ClearPredictionBuffer(Duration);
	Presage->BeginSimulation();

	// Freeze movement components so the AI cannot move the actor during the synchronous bake.
	auto* MoveComp = Actor->FindComponentByClass<UCharacterMovementComponent>();
	if (MoveComp)
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetComponentTickEnabled(false);
	}

	if (const auto* Pawn = Cast<APawn>(Actor))
	{
		if (const auto* AIC = Cast<AAIController>(Pawn->GetController()))
		{
			if (auto* PFC = AIC->GetPathFollowingComponent())
			{
				PFC->SetComponentTickEnabled(false);
			}
		}
	}
}

void FWolfPresageSimulator::BakeSimulationStep(const TScriptInterface<IWolfCombatant>& Combatant, float StepSize)
{
	const auto* Actor = Cast<AActor>(Combatant.GetObject());
	if (!IsValid(Actor)) return;

	auto* Presage = Combatant.GetInterface()->GetPresageComponent();
	if (!IsValid(Presage)) return;

	Presage->SimulateTick(StepSize);
}

void FWolfPresageSimulator::CleanupCombatantSimulation(const TScriptInterface<IWolfCombatant>& Combatant)
{
	const auto* Actor = Cast<AActor>(Combatant.GetObject());
	if (!IsValid(Actor)) return;

	auto* Presage = Combatant.GetInterface()->GetPresageComponent();
	if (IsValid(Presage))
	{
		Presage->EndSimulation();
	}

	// Re-enable movement ticks. The subsequent ScrubTimeline(0.f) in SetMode
	// will restore the actor to its pre-bake position via MasterStartSnapshot.
	auto* MoveComp = Actor->FindComponentByClass<UCharacterMovementComponent>();
	if (MoveComp)
	{
		MoveComp->SetComponentTickEnabled(true);
	}

	if (const auto* Pawn = Cast<APawn>(Actor))
	{
		if (auto* AIC = Cast<AAIController>(Pawn->GetController()))
		{
			if (auto* PFC = AIC->GetPathFollowingComponent())
			{
				PFC->SetComponentTickEnabled(true);
			}
		}
	}
}

void FWolfPresageSimulator::ApplyModeDrainEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode, FGameplayTag DrainActiveInMode, TSubclassOf<UGameplayEffect> DrainEffectClass)
{
	if (!ASC || !DrainEffectClass) return;

	// Present iff the actor's current mode is the drain's active mode — the only mode-dependent bit
	// of the retired presage drain, parameterized so RT Adrenaline drain and any future mode-gated
	// drain share one add/remove structure (ResourceLoop stage 4 generalization).
	const bool bShouldHaveEffect = CurrentActorMode == DrainActiveInMode;

	FGameplayEffectQuery Query;
	Query.EffectDefinition = DrainEffectClass;

	const TArray<FActiveGameplayEffectHandle> ActiveHandles = ASC->GetActiveEffects(Query);
	const bool bEffectAlreadyApplied = ActiveHandles.Num() > 0;

	if (bShouldHaveEffect == bEffectAlreadyApplied) return;

	if (bShouldHaveEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(ASC->GetAvatarActor(), ASC->GetAvatarActor());

		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(DrainEffectClass, 1.f, Context);
		if (SpecHandle.IsValid()) ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	else ASC->RemoveActiveGameplayEffectBySourceEffect(DrainEffectClass, nullptr);
}