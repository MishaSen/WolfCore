#include "Presage/PresageOrchestrator.h"

#include "Systems/CombatModeSubsystem.h"
#include "Abilities/BaseCombatAbility.h"
#include "Core/WolfCombatSettings.h"
#include "Core/WolfPresageComponent.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

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

		// AI combatant: chain valid abilities back-to-back until it covers the full duration.
		// Real disposition/negotiation/interrupts are not implemented yet (stage 5 for negotiation;
		// interrupts as of this stage are detected but only "take the hit" is functional).
		AActor* StubTarget = nullptr;
		for (const auto& Other : Combatants)
		{
			const auto* OtherActor = Cast<AActor>(Other.GetObject());
			if (IsValid(OtherActor) && OtherActor != Actor)
			{
				StubTarget = const_cast<AActor*>(OtherActor);
				break;
			}
		}

		float TimeCursor = 0.f;
		while (TimeCursor < Duration)
		{
			const auto AbilityClass = DecideIntent(Combatant);
			if (!AbilityClass) break; // nothing configured — leave this combatant with no plan.

			FIntentEntry Entry;
			Entry.Combatant = Combatant;
			Entry.AbilityClass = AbilityClass;
			Entry.Timing = CMS->GetOrComputeTimingProfile(AbilityClass);
			Entry.StartTime = TimeCursor;
			Entry.Status = EIntentStatus::Confirmed;
			Entry.Target = StubTarget; // STAGE 4 STUB — first other tracked combatant, not real targeting.
			Ledger.Add(Entry);

			// Guard against a zero-duration ability looping forever.
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

void FPresageOrchestrator::ResolveInterrupts(TArray<FIntentEntry>& Ledger)
{
	// Map from victim index -> earliest attacker's AttackWindowStart that hits them. A victim can
	// overlap with more than one attacker; only the earliest one actually lands first and
	// interrupts them — later ones are moot for this entry (stacking further damage/hitstun
	// beyond the first hit is a separate, out-of-scope concern). No iteration cap is needed here:
	// that only becomes relevant once a *response* (e.g. a future dodge/parry) has its own active
	// window that could itself be interrupted by a second attacker — "take the hit" has no such
	// window, so there's nothing to recurse into yet.
	TMap<int32, float> EarliestAttackTime;

	for (int32 i = 0; i < Ledger.Num(); ++i)
	{
		const FIntentEntry& Attacker = Ledger[i];
		if (!Attacker.Target.IsValid()) continue;

		const float AttackWindowStart = Attacker.StartTime + Attacker.Timing.ActiveWindowStart;
		const float AttackWindowEnd = Attacker.StartTime + Attacker.Timing.ActiveWindowEnd;

		for (int32 j = 0; j < Ledger.Num(); ++j)
		{
			if (i == j) continue;

			const FIntentEntry& Victim = Ledger[j];
			const auto* VictimActor = Cast<AActor>(Victim.Combatant.GetObject());
			if (!IsValid(VictimActor) || VictimActor != Attacker.Target.Get()) continue;

			const float VictimWindupEnd = Victim.StartTime + Victim.Timing.WindupDuration;

			// Interval overlap: attacker's active window intersects victim's windup.
			if (AttackWindowStart < VictimWindupEnd && AttackWindowEnd > Victim.StartTime)
			{
				if (const float* Existing = EarliestAttackTime.Find(j))
				{
					if (AttackWindowStart < *Existing)
					{
						EarliestAttackTime[j] = AttackWindowStart;
					}
				}
				else
				{
					EarliestAttackTime.Add(j, AttackWindowStart);
				}
			}
		}
	}

	// Resolve each interrupted victim exactly once, using the earliest incoming attack.
	for (const auto& Pair : EarliestAttackTime)
	{
		const int32 VictimIndex = Pair.Key;
		const float EarliestAttackWindowStart = Pair.Value;

		FIntentEntry& Entry = Ledger[VictimIndex];

		const TArray<FInterruptResponseOption> Options = GetAvailableResponses(Entry);
		const FInterruptResponseOption* Chosen = PickWeightedResponse(Options);

		if (Chosen)
		{
			WOLF_LOG(Log, TEXT("[PRESAGE] Interrupt response chosen: %s (only take-hit is mechanically implemented this stage)"),
				*Chosen->ResponseTag.ToString());
		}

		// Only "take the hit" is functional — regardless of which option (if any) was chosen,
		// the mechanical effect this stage implements is: cut the interrupted ability short at
		// the moment the earliest interrupting attack actually lands.
		Entry.bHasInterruptedAtTime = true;
		Entry.InterruptedAtTime = FMath::Max(Entry.StartTime, EarliestAttackWindowStart);
		Entry.Status = EIntentStatus::Confirmed;
	}
}

TArray<FInterruptResponseOption> FPresageOrchestrator::GetAvailableResponses(const FIntentEntry& InterruptedEntry)
{
	TArray<FInterruptResponseOption> Result;

	const auto* AbilityCDO = InterruptedEntry.AbilityClass
		? InterruptedEntry.AbilityClass->GetDefaultObject<UBaseCombatAbility>()
		: nullptr;

	const TArray<FInterruptResponseOption>* Source = nullptr;
	if (AbilityCDO && AbilityCDO->InterruptResponses.Num() > 0)
	{
		Source = &AbilityCDO->InterruptResponses;
	}
	else if (const auto* Settings = GetDefault<UWolfCombatSettings>())
	{
		Source = &Settings->DefaultInterruptResponses;
	}

	if (Source)
	{
		for (const auto& Option : *Source)
		{
			if (Option.RequiredTags.IsEmpty()) // stage 4 simplification — see header comment
			{
				Result.Add(Option);
			}
		}
	}

	return Result;
}

const FInterruptResponseOption* FPresageOrchestrator::PickWeightedResponse(const TArray<FInterruptResponseOption>& Options)
{
	if (Options.Num() == 0) return nullptr;

	float TotalWeight = 0.f;
	for (const auto& Option : Options) TotalWeight += FMath::Max(Option.Weight, 0.f);
	if (TotalWeight <= 0.f) return &Options[0];

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (const auto& Option : Options)
	{
		Roll -= FMath::Max(Option.Weight, 0.f);
		if (Roll <= 0.f) return &Option;
	}
	return &Options.Last();
}

TSubclassOf<UBaseCombatAbility> FPresageOrchestrator::DecideIntent(const TScriptInterface<IWolfCombatant>& Combatant)
{
	const auto* Actor = Cast<AActor>(Combatant.GetObject());
	if (!IsValid(Actor)) return nullptr;

	const auto* ASI = Cast<IAbilitySystemInterface>(Actor);
	UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;
	if (!ASC) return nullptr;

	TArray<TSubclassOf<UBaseCombatAbility>> ValidCandidates;

	FScopedAbilityListLock ActiveScopeLock(*ASC);
	for (const auto& Spec : ASC->GetActivatableAbilities())
	{
		UBaseCombatAbility* CombatAbility = Cast<UBaseCombatAbility>(Spec.Ability);
		if (!CombatAbility) continue; // not a combat ability (e.g. a passive/buff-only ability)

		if (!CombatAbility->CanActivateAbility(Spec.Handle, ASC->AbilityActorInfo.Get()))
		{
			continue; // blocked by tags, on cooldown, insufficient cost — same real gating, no
			          // special-cased orchestrator rule.
		}

		ValidCandidates.Add(CombatAbility->GetClass());
	}

	if (ValidCandidates.Num() == 0) return nullptr;

	return ValidCandidates[FMath::RandRange(0, ValidCandidates.Num() - 1)];
}