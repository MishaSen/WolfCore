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
	PerformAttackTrace(Avatar, StartVector, EndVector, Hits);

	bool bValidTargetFound = false;

	for (const FHitResult& Hit : Hits)
	{
		if (ProcessAttackHit(Hit, ContextPeriod, Avatar))
		{
			bValidTargetFound = true;
		}
	}

	DrawAttackDebugCylinder(Avatar, StartVector, EndVector, AttackRadius, bValidTargetFound);
}

void URTCombatAbility::PerformAttackTrace(AActor* Avatar, const FVector& Start, const FVector& End, TArray<FHitResult>& OutHits)
{
	// TODO: Consider filtering additional actors (invulnerable targets, teammates, dead characters)
	UKismetSystemLibrary::SphereTraceMulti(
		Avatar, Start, End, AttackRadius,
		UEngineTypes::ConvertToTraceType(ECC_Pawn), false, {Avatar},
		EDrawDebugTrace::None, OutHits, false
	);
}

bool URTCombatAbility::ProcessAttackHit(const FHitResult& Hit, const FCombatPeriod& ContextPeriod, const AActor* Avatar)
{
	auto* TargetChar = Cast<AWolfCharacterBase>(Hit.GetActor());
	if (!TargetChar || TargetChar == Avatar)
	{
		return false;
	}

	auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetChar);
	if (!TargetASC) return false;

	WOLF_INFO("Hit character: %s", *TargetChar->GetName());
	ApplyHitEffects(ContextPeriod, TargetChar, &Hit);

	return true;
}

void URTCombatAbility::DrawAttackDebugCylinder(const AActor* Avatar, const FVector& StartPos, const FVector& EndPos, float Radius, bool bValidTargetFound)
{
	const FColor CylinderColor = bValidTargetFound ? FColor::Green : FColor::Red;
	constexpr float Duration = 3.0f;

	DrawDebugCylinder(
		Avatar->GetWorld(),
		StartPos, EndPos, Radius, 12,
		CylinderColor, false, Duration, 0, 1.0f
	);
}