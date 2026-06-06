// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/RTCombatAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/AbilityFrameData.h"
#include "Debug/WolfDebug.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Systems/CombatModeSubsystem.h"
#include "Character/WolfCharacterBase.h"

void URTCombatAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo,
                                       const FGameplayAbilityActivationInfo ActivationInfo,
                                       const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	StartCombatSequence();
}

void URTCombatAbility::HandleAttackHitEvent(const FCombatPeriod& ContextPeriod)
{
	auto* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	const auto StartVector = Avatar->GetActorLocation();
	WOLF_INFO(TEXT("Actor location is at %s"), *StartVector.ToString());
	const auto EndVector = StartVector + Avatar->GetActorForwardVector() * AttackRange;

	TArray<FHitResult> Hits;
	UKismetSystemLibrary::SphereTraceMulti(
		this, StartVector, EndVector, AttackRadius,
		UEngineTypes::ConvertToTraceType(ECC_Pawn), false, {Avatar},
		EDrawDebugTrace::ForDuration, Hits, true, FLinearColor::Black, FLinearColor::Blue
	);

	auto* MyASC = GetAbilitySystemComponentFromActorInfo();
	if (!MyASC) return;

	const auto AbilityLevel = GetAbilityLevel();

	for (const FHitResult& Hit : Hits)
	{
		if (!Hit.GetActor() || Hit.GetActor() == Avatar) continue;

		auto* TargetChar = Cast<AWolfCharacterBase>(Hit.GetActor());
		if (!TargetChar)
		{
			WOLF_INFO("Hit actor is not a WolfCharacterBase: %s", *Hit.GetActor()->GetName());
			continue;
		}

		WOLF_INFO("Hit character: %s", *TargetChar->GetName());
		auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetChar);
		if (!TargetASC) continue;

		auto EffectContextHandle = MyASC->MakeEffectContext();
		EffectContextHandle.AddHitResult(Hit);

		auto ApplyEffect = [&](const TSubclassOf<UGameplayEffect>& EffectClass, const FScalableFloat& AttributeAmount,
		                       bool bToTarget, FGameplayTag DataAmountTag = FWolfGameplayTags::Get().Data_Amount)
		{
			if (!EffectClass) return;

			const auto EffectSpecHandle = MyASC->MakeOutgoingSpec(EffectClass, AbilityLevel, EffectContextHandle);
			EffectSpecHandle.Data->SetSetByCallerMagnitude(DataAmountTag, AttributeAmount.GetValueAtLevel(AbilityLevel));

			if (bToTarget) MyASC->ApplyGameplayEffectSpecToTarget(*EffectSpecHandle.Data.Get(), TargetASC);
			else MyASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
		};

		ApplyEffect(FlowGainEffect, ContextPeriod.FlowGain, false);
		ApplyEffect(AdrenalineGainEffect, ContextPeriod.AdrenalineGain, false);
		ApplyEffect(DamageEffect, ContextPeriod.Damage, true);
	}
}
