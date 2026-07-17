#include "Presage/PresageOrchestrator.h"

#include "Systems/CombatModeSubsystem.h"
#include "Abilities/BaseCombatAbility.h"
#include "Core/WolfCombatSettings.h"
#include "Core/WolfPresageComponent.h"
#include "GameFramework/Pawn.h"

TSubclassOf<UBaseCombatAbility> FPresageOrchestrator::DecideIntent_Stub()
{
	const auto* Settings = GetDefault<UWolfCombatSettings>();
	return Settings ? Settings->DefaultPresageStubAbility : nullptr;
}

TArray<FIntentEntry> FPresageOrchestrator::RunPlanning(
	UCombatModeSubsystem* CMS,
	const TArray<TScriptInterface<IWolfCombatant>>& Combatants,
	float Duration)
{
	TArray<FIntentEntry> Ledger;
	if (!CMS) return Ledger;

	for (const auto& Combatant : Combatants)
	{
		const auto* Actor = Cast<AActor>(Combatant.GetObject());
		if (!IsValid(Actor)) continue;

		auto* Presage = Combatant.GetInterface() ? Combatant.GetInterface()->GetPresageComponent() : nullptr;
		if (!IsValid(Presage)) continue;

		const auto* Pawn = Cast<APawn>(Actor);
		const bool bIsPlayer = Pawn && Pawn->IsPlayerControlled();

		if (bIsPlayer)
		{
			// Player's own intent comes from the existing injection mechanism, not the orchestrator.
			// At most one entry — Execution does not auto-continue the player past it (stage 3 scope).
			if (Presage->HasInjectedAbilityRequest())
			{
				const auto& Request = Presage->GetInjectedAbilityRequest();

				FIntentEntry Entry;
				Entry.Combatant = Combatant;
				Entry.AbilityClass = Request.AbilityClass;
				Entry.Timing = CMS->GetOrComputeTimingProfile(Request.AbilityClass);
				Entry.StartTime = Request.GetScheduledTime();
				Entry.Status = EIntentStatus::Confirmed;

				for (const auto& WeakTarget : Request.GetTargets())
				{
					if (WeakTarget.IsValid())
					{
						Entry.Target = WeakTarget;
						break;
					}
				}

				Ledger.Add(Entry);
			}
			continue;
		}

		// AI combatant: chain the stub ability back-to-back until it covers the full duration.
		// Real disposition/negotiation/interrupts are not implemented yet (stage 4/5).
		float TimeCursor = 0.f;
		while (TimeCursor < Duration)
		{
			const auto AbilityClass = DecideIntent_Stub();
			if (!AbilityClass) break; // nothing configured — leave this combatant with no plan.

			FIntentEntry Entry;
			Entry.Combatant = Combatant;
			Entry.AbilityClass = AbilityClass;
			Entry.Timing = CMS->GetOrComputeTimingProfile(AbilityClass);
			Entry.StartTime = TimeCursor;
			Entry.Status = EIntentStatus::Confirmed;
			Ledger.Add(Entry);

			// Guard against a zero-duration stub ability looping forever.
			TimeCursor += FMath::Max(Entry.Timing.TotalDuration, KINDA_SMALL_NUMBER);
		}
	}

	return Ledger;
}

void FPresageOrchestrator::DistributePlan(
	const TArray<TScriptInterface<IWolfCombatant>>& Combatants,
	const TArray<FIntentEntry>& Plan)
{
	for (const auto& Combatant : Combatants)
	{
		auto* Presage = Combatant.GetInterface() ? Combatant.GetInterface()->GetPresageComponent() : nullptr;
		if (!IsValid(Presage)) continue;

		TArray<FIntentEntry> ForThisCombatant = Plan.FilterByPredicate(
			[&Combatant](const FIntentEntry& Entry) { return Entry.Combatant == Combatant; });

		ForThisCombatant.Sort([](const FIntentEntry& A, const FIntentEntry& B) { return A.StartTime < B.StartTime; });

		Presage->SetPlannedIntents(ForThisCombatant);
	}
}