// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCore/Public/Core/WolfPresageSimulator.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BaseCombatAbility.h"
#include "Core/WolfGameplayTags.h"
#include "Core/WolfPresageComponent.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/IWolfCombatant.h"
#include "Presage/ActorSnapshot.h"

void FWolfPresageSimulator::ExecuteFutureBake(const TArray<TScriptInterface<IWolfCombatant>>& Combatants, float Duration)
{
	if (Combatants.Num() == 0 || FMath::IsNearlyEqual(Duration, 0.f)) return;

	// Phase 1: Setup each combatant's simulation state.
	for (auto& Combatant : Combatants)
	{
		SetupCombatantSimulation(Combatant, Duration);
	}

	// Phase 2: Bake simulation steps.
	constexpr float Step = WolfSimConfig::Step;
	const int32 TotalSteps = FMath::CeilToInt(Duration / Step);

	WOLF_LOG(Log, TEXT("Baking Future: %d steps over %.2fs"), TotalSteps, Duration);

	for (int32 i = 0; i < TotalSteps; ++i)
	{
		WOLF_LOG(Log, TEXT("[SIM] Step %d"), i);
		for (auto& Combatant : Combatants)
		{
			BakeSimulationStep(Combatant, Step);
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

	// Clear prediction buffer and enable simulation.
	Presage->ClearPredictionBuffer(Duration);
	Presage->SetIsSimulating(true);
	Presage->SetSimulationTransform(Actor->GetActorTransform());

	// Sync sim timer to the current period from active abilities via interface.
	const auto* ASC = Combatant.GetInterface()->GetASC();
	if (ASC)
	{
		const float CurrentProgress = Combatant.GetInterface()->GetActiveAbilityProgress();
		Presage->SimPeriodTime = CurrentProgress;
	}

	// Stop character movement during simulation.
	auto* MoveComp = Actor->FindComponentByClass<UCharacterMovementComponent>();
	if (MoveComp) MoveComp->StopMovementImmediately();
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
	if (!IsValid(Presage)) return;

	Presage->SetIsSimulating(false);
}

void FWolfPresageSimulator::ApplyPresageDrainEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode, TSubclassOf<UGameplayEffect> PresageEffectClass)
{
	if (!ASC || !PresageEffectClass) return;

	const bool bShouldHaveEffect = CurrentActorMode == FWolfGameplayTags::Get().InputState_TB;

	FGameplayEffectQuery Query;
	Query.EffectDefinition = PresageEffectClass;

	const TArray<FActiveGameplayEffectHandle> ActiveHandles = ASC->GetActiveEffects(Query);
	const bool bEffectAlreadyApplied = ActiveHandles.Num() > 0;

	if (bShouldHaveEffect == bEffectAlreadyApplied) return;

	if (bShouldHaveEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(ASC->GetAvatarActor(), ASC->GetAvatarActor());

		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(PresageEffectClass, 1.f, Context);
		if (SpecHandle.IsValid()) ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	else ASC->RemoveActiveGameplayEffectBySourceEffect(PresageEffectClass, nullptr);
}