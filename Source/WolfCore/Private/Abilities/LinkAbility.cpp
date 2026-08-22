// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/LinkAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/WolfAttributeSet.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "Interfaces/IWolfCombatant.h"

bool ULinkAbility::ProcessAttackHit(const FHitResult& Hit, const FCombatPeriod& ContextPeriod, const AActor* Avatar)
{
	AActor* TargetActor = Hit.GetActor();
	if (!TargetActor || TargetActor == Avatar)
	{
		return false;
	}

	auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC) return false;

	const auto& WolfTag = FWolfGameplayTags::Get();
	if (TargetASC->HasMatchingGameplayTag(WolfTag.Status_Link))
	{
		// Re-linking refreshes nothing this pass — refresh-on-relink is a design question for
		// when links matter to gameplay, not now.
		return false;
	}

	// Eligibility: link any IWolfCombatant hit. Vision restricts links to allies; with no team
	// system, there is nothing to filter on.
	// TODO(teams): enemies are currently linkable, for testing only; the team filter goes here.
	if (!Cast<IWolfCombatant>(TargetActor))
	{
		return false;
	}

	auto* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC) return false;

	const float CurrentFlow = SourceASC->GetNumericAttribute(UWolfAttributeSet::GetFlowGaugeAttribute());
	if (CurrentFlow < FlowCostPerLink)
	{
		WOLF_INFO(TEXT("[LinkAbility] Insufficient Flow (%f < required %f) — no further hits will link this activation."),
			CurrentFlow, FlowCostPerLink);
		return false;
	}

	const auto AbilityLevel = GetAbilityLevel();

	// Deduct the Flow cost via the same instant SetByCaller GE pattern ApplyHitEffects uses.
	if (FlowCostEffectClass)
	{
		auto CostContext = SourceASC->MakeEffectContext();
		const auto CostSpecHandle = SourceASC->MakeOutgoingSpec(FlowCostEffectClass, AbilityLevel, CostContext);
		if (CostSpecHandle.IsValid())
		{
			CostSpecHandle.Data->SetSetByCallerMagnitude(FWolfGameplayTags::Get().Data_Amount, -FlowCostPerLink);
			SourceASC->ApplyGameplayEffectSpecToSelf(*CostSpecHandle.Data.Get());
		}
	}
	else
	{
		WOLF_WARN(TEXT("[LinkAbility] FlowCostEffectClass is unset — no Flow was deducted for this link."));
	}

	// Apply the link effect. Each application is an independent GE instance, so per-link
	// independent duration falls out of GAS for free.
	if (LinkEffectClass)
	{
		auto LinkContext = SourceASC->MakeEffectContext();
		LinkContext.AddHitResult(Hit);
		const auto LinkSpecHandle = SourceASC->MakeOutgoingSpec(LinkEffectClass, AbilityLevel, LinkContext);
		if (LinkSpecHandle.IsValid())
		{
			LinkSpecHandle.Data->SetDuration(LinkDurationSeconds, false);
			SourceASC->ApplyGameplayEffectSpecToTarget(*LinkSpecHandle.Data.Get(), TargetASC);
		}
	}
	else
	{
		WOLF_WARN(TEXT("[LinkAbility] LinkEffectClass is unset — no link was actually applied."));
		return false;
	}

	WOLF_INFO(TEXT("[LinkAbility] Linked %s (Flow cost %f, duration %f)."),
		*TargetActor->GetName(), FlowCostPerLink, LinkDurationSeconds);
	return true;
}