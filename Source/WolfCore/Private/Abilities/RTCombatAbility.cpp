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

	const FVector StartVector = Avatar->GetActorLocation();
	WOLF_INFO(TEXT("Actor location is at %s"), *StartVector.ToString());
	const FVector EndVector = StartVector + Avatar->GetActorForwardVector() * AttackRange;

	TArray<FHitResult> Hits;
	UKismetSystemLibrary::SphereTraceMulti(
		this, StartVector, EndVector, AttackRadius,
		UEngineTypes::ConvertToTraceType(ECC_Pawn), false, {Avatar},
		EDrawDebugTrace::None, Hits, false
	);

	auto* MyASC = GetAbilitySystemComponentFromActorInfo();
	if (!MyASC) return;

	const auto AbilityLevel = GetAbilityLevel();
	bool bValidTargetFound = false;

	for (const FHitResult& Hit : Hits)
	{
		auto* TargetChar = Cast<AWolfCharacterBase>(Hit.GetActor());
		if (!TargetChar || TargetChar == Avatar)
		{
			continue;
		}

		auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetChar);
		if (!TargetASC) continue;

		bValidTargetFound = true;
		WOLF_INFO("Hit character: %s", *TargetChar->GetName());

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

	DrawAttackDebugCylinder(StartVector, EndVector, AttackRadius, bValidTargetFound);
}

void URTCombatAbility::DrawAttackDebugCylinder(const FVector& StartPos, const FVector& EndPos, float Radius, bool bValidTargetFound) const
{
	const FColor CylinderColor = bValidTargetFound ? FColor::Green : FColor::Red;
	constexpr float Duration = 3.0f;

	DrawDebugCylinder(
		GetAvatarActorFromActorInfo()->GetWorld(),
		StartPos, EndPos, Radius, 12,
		CylinderColor, false, Duration, 0, 1.0f
	);
}
