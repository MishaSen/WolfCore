// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/RTCombatAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/AbilityFrameData.h"
#include "Debug/WolfDebug.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Systems/CombatModeSubsystem.h"

void URTCombatAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo,
                                       const FGameplayAbilityActivationInfo ActivationInfo,
                                       const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	StartCombatSequence();
}

void URTCombatAbility::HandleAttackHitEvent()
{
	auto* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	const auto StartVector = Avatar->GetActorLocation();
	const auto EndVector = StartVector + Avatar->GetActorForwardVector() * AttackRange;

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(Avatar);

	FHitResult HitResult;
	const auto bHit = UKismetSystemLibrary::SphereTraceSingle(
		this, StartVector, EndVector, AttackRadius,
		UEngineTypes::ConvertToTraceType(ECC_Pawn), false, IgnoreActors,
		EDrawDebugTrace::ForDuration,HitResult, true
		);

	if (bHit && HitResult.GetActor())
	{
		auto* MyASC = GetAbilitySystemComponentFromActorInfo();
		auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitResult.GetActor());

		if (!MyASC)
		{
			WOLF_WARN(TEXT("No AbilitySystemComponent found on Avatar Actor."));
			return;
		}
		if (!TargetASC)
		{
			WOLF_WARN(TEXT("No AbilitySystemComponent found on Target Actor."));
			return;
		}

		if (FlowGainEffect)
		{
			const auto SpecHandle = MyASC->MakeOutgoingSpec(FlowGainEffect, 1.f, MyASC->MakeEffectContext());
			SpecHandle.Data->SetSetByCallerMagnitude(FWolfGameplayTags::Get().Data_Amount, 10.f);
			MyASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
		if (AdrenalineGainEffect)
		{
			const auto SpecHandle = TargetASC->MakeOutgoingSpec(AdrenalineGainEffect, 1.f, TargetASC->MakeEffectContext());
			SpecHandle.Data->SetSetByCallerMagnitude(FWolfGameplayTags::Get().Data_Amount, 10.f);
			TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
		if (DamageEffect)
		{
			MyASC->ApplyGameplayEffectToTarget(DamageEffect.GetDefaultObject(), TargetASC, 1.f);
		}
	}
}