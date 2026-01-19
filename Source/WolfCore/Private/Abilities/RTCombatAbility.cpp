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

	CurrentPeriodIndex = 0;
	PlayNextPeriod();
}

void URTCombatAbility::PlayNextPeriod()
{
	if (!AbilitySequence.IsValidIndex(CurrentPeriodIndex))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const FCombatPeriod& Period = AbilitySequence[CurrentPeriodIndex];

	if (Period.Type == EPeriodType::Attack)
	{
		FGameplayTag Tag = FWolfGameplayTags::Get().Event_Ability_Attack;

		UAbilityTask_WaitGameplayEvent* WaitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, Tag);

		WaitTask->EventReceived.AddDynamic(this, &URTCombatAbility::OnEventReceived);
		WaitTask->ReadyForActivation();
	}

	if (Period.Montage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				NAME_None,
				Period.Montage,
				1.f,
				NAME_None,
				false,
				1.f,
				0.f,
				false
			);

		MontageTask->OnCompleted.AddDynamic(this, &URTCombatAbility::OnPeriodCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &URTCombatAbility::K2_EndAbility);
		MontageTask->OnCancelled.AddDynamic(this, &URTCombatAbility::K2_EndAbility);

		MontageTask->ReadyForActivation();
	}
	else // For prototyping
	{
		float Duration = Period.Duration > 0.f ? Period.Duration : 0.1f;
		if (Period.Type == EPeriodType::Attack)
		{
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &URTCombatAbility::PerformAttackTrace,
			                                       Period.HitDelay, false);
		}

		UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
		DelayTask->OnFinish.AddDynamic(this, &URTCombatAbility::OnPeriodCompleted);
		DelayTask->ReadyForActivation();
	}
}

void URTCombatAbility::OnPeriodCompleted()
{
	CurrentPeriodIndex++;
	PlayNextPeriod();
}

void URTCombatAbility::OnEventReceived(FGameplayEventData EventData)
{
	PerformAttackTrace();
}

void URTCombatAbility::PerformAttackTrace()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	FVector Start = Avatar->GetActorLocation();
	FVector End = Start + Avatar->GetActorForwardVector() * AttackRange;

	TArray<AActor*> Ignore;
	Ignore.Add(Avatar);

	FHitResult HitResult;
	bool bHit = UKismetSystemLibrary::SphereTraceSingle(
		this, Start, End, AttackRadius,
		UEngineTypes::ConvertToTraceType(ECC_Pawn),
		false, Ignore,
		EDrawDebugTrace::ForDuration,
		HitResult, true
		);

	if (bHit && HitResult.GetActor())
	{
		UAbilitySystemComponent* MyASC = GetAbilitySystemComponentFromActorInfo();
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitResult.GetActor());

		if (MyASC)
		{
			if (FlowGainEffect)
			{
				FGameplayEffectSpecHandle Spec = MyASC->MakeOutgoingSpec(FlowGainEffect, 1.f, MyASC->MakeEffectContext());
				Spec.Data->SetSetByCallerMagnitude(FWolfGameplayTags::Get().Data_Amount, 10.f);
				MyASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}

			if (TargetASC && DamageEffect)
			{
				MyASC->ApplyGameplayEffectToTarget(DamageEffect.GetDefaultObject(), TargetASC, 1.f);
			}
		}
	}
}

