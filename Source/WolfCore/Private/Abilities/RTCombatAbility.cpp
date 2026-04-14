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
	if (!AbilitySequence.IsValidIndex(CurrentPeriodIndex))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		WOLF_ERROR("CombatSequence index out of bounds.");
		return;
	}

	const auto& CurrentPeriod = AbilitySequence[CurrentPeriodIndex];

	auto* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	const auto StartVector = Avatar->GetActorLocation();
	WOLF_INFO(TEXT("Actor location is at %s"), *StartVector.ToString());
	const auto EndVector = StartVector + Avatar->GetActorForwardVector() * AttackRange;

	FHitResult HitResult;
	const auto bHit = UKismetSystemLibrary::SphereTraceSingle(
		this, StartVector, EndVector, AttackRadius,
		UEngineTypes::ConvertToTraceType(ECC_Pawn), false, {Avatar},
		EDrawDebugTrace::ForDuration, HitResult, true
	);

	if (bHit && HitResult.GetActor())
	{
		auto* MyASC = GetAbilitySystemComponentFromActorInfo();
		auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitResult.GetActor());
		if (!MyASC || !TargetASC) return;

		auto EffectContextHandle = MyASC->MakeEffectContext();
		EffectContextHandle.AddHitResult(HitResult);

		const auto AbilityLevel = GetAbilityLevel();

		auto ApplyEffect = [&](const TSubclassOf<UGameplayEffect>& EffectClass, const FScalableFloat& AttributeAmount,
		                       bool bToTarget, FGameplayTag DataAmountTag = FWolfGameplayTags::Get().Data_Amount)
		{
			if (!EffectClass) return;

			const auto EffectSpecHandle = MyASC->MakeOutgoingSpec(EffectClass, AbilityLevel, EffectContextHandle);
			EffectSpecHandle.Data->SetSetByCallerMagnitude(DataAmountTag, AttributeAmount.GetValueAtLevel(AbilityLevel));

			if (bToTarget) MyASC->ApplyGameplayEffectSpecToTarget(*EffectSpecHandle.Data.Get(), TargetASC);
			else MyASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
		};

		ApplyEffect(FlowGainEffect, CurrentPeriod.FlowGain, false);
		ApplyEffect(AdrenalineGainEffect, CurrentPeriod.AdrenalineGain, false);
		ApplyEffect(DamageEffect, CurrentPeriod.Damage, true);
	}
}