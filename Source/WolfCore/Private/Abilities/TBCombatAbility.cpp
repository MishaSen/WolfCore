// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Abilities/TBCombatAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "DrawDebugHelpers.h"
#include "Abilities/Notifies/AnimNotify_Hit.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Debug/WolfDebug.h"

void UTBCombatAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo,
                                       const FGameplayAbilityActivationInfo ActivationInfo,
                                       const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	StartCombatSequence();
}

void UTBCombatAbility::PostInitProperties()
{
	Super::PostInitProperties();

	CachedHitTime = CalculateProjectedImpactTime();
}