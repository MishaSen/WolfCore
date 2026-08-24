// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Abilities/BaseCombatAbility.h"

#include "AIController.h"
#include "Engine.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Notifies/AnimNotify_Hit.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/WolfCharacterBase.h"
#include "Core/WolfGameplayTags.h"
#include "Core/WolfResourceRules.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Abilities/Tasks/AbilityTask_MoveToLocation.h"
#include "Systems/CombatModeSubsystem.h"

UBaseCombatAbility::UBaseCombatAbility()
{
	ActivationBlockedTags.AddTag(FWolfGameplayTags::Get().InputState_Dead);
	CancelAbilitiesWithTag.AddTag(FWolfGameplayTags::Get().InputState_Dead);
}

void UBaseCombatAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	OnAbilityActivated.Broadcast(this);
}

void UBaseCombatAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	OnAbilityDeactivated.Broadcast(this);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}


void UBaseCombatAbility::StartCombatSequence()
{
	CurrentPeriodIndex = 0;
	WOLF_LOG(Log, TEXT("Combat sequence started with %d periods."), AbilitySequence.Num());
	PlayNextPeriod();
}

void UBaseCombatAbility::ExecuteWait(const FCombatPeriod& Period)
{
	/* Consider implementing something like ApplyWaitTags() for wait period behavior. E.g., take more damage it hit.*/
	if (IsValid(Period.Montage)) { ExecuteAnimatedPeriod(Period); }
	else
	{
		auto* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Period.Duration);
		DelayTask->OnFinish.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		DelayTask->ReadyForActivation();
	}
}

void UBaseCombatAbility::ExecuteAnimatedPeriod(const FCombatPeriod& Period)
{
	if (IsValid(Period.Montage)) // Currently does not support non attack period montages.
	{
		auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Period.Montage, 1.f, NAME_None, false,
				1.f, 0.f, false);

		if (Period.Type == EPeriodType::Attack && HitEventTag.IsValid())
		{
			auto* WaitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
				this, HitEventTag, nullptr);
			
			WaitTask->EventReceived.AddDynamic(this, &UBaseCombatAbility::OnEventReceived);

			MontageTask->OnCompleted.AddDynamic(WaitTask, &UAbilityTask_WaitGameplayEvent::EndTask);
			MontageTask->OnInterrupted.AddDynamic(WaitTask, &UAbilityTask_WaitGameplayEvent::EndTask);
			MontageTask->OnCancelled.AddDynamic(WaitTask, &UAbilityTask_WaitGameplayEvent::EndTask);

			WaitTask->ReadyForActivation();
		}

		MontageTask->OnCompleted.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UBaseCombatAbility::K2_EndAbility);
		MontageTask->OnCancelled.AddDynamic(this, &UBaseCombatAbility::K2_EndAbility);
		MontageTask->ReadyForActivation();
	}
	else
	{
		if (Period.Type == EPeriodType::Attack)
		{
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle,
				[this, Period]() { HandleAttackHitEvent(Period); },
				Period.HitDelay, false);
		}

		auto* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Period.Duration);
		DelayTask->OnFinish.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		DelayTask->ReadyForActivation();
	}
}

void UBaseCombatAbility::PlayNextPeriod()
{
	if (!AbilitySequence.IsValidIndex(CurrentPeriodIndex))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (const auto* World = GetWorld()) CurrentPeriodStartTime = World->GetTimeSeconds();

	auto& CombatPeriod = AbilitySequence[CurrentPeriodIndex];
	switch (CombatPeriod.Type)
	{
		case EPeriodType::MoveTo: ExecuteMoveTo(CombatPeriod); break;
		case EPeriodType::Wait:	  ExecuteWait(CombatPeriod);   break;
		
		case EPeriodType::Attack:
		case EPeriodType::Windup:
		case EPeriodType::Evasion: ExecuteAnimatedPeriod(CombatPeriod); break;
	}
}

float UBaseCombatAbility::GetPeriodProgress() const
{
	if (!AbilitySequence.IsValidIndex(CurrentPeriodIndex)) return 0.f;

	const auto* World = GetWorld();
	if (!World) return 0.f;

	const auto ElapsedTime = World->GetTimeSeconds() - CurrentPeriodStartTime;
	return FMath::Max(0.f, ElapsedTime);
}

void UBaseCombatAbility::ExecuteMoveTo(FCombatPeriod& Period)
{
	const auto* Target = GetTargetFromBlackboard();
	const auto* AvatarActor = GetAvatarActorFromActorInfo();
	if (!Target || !AvatarActor) { OnPeriodCompleted(); return; }

	WOLF_LOG(Log, TEXT("Executing MoveTo for ability %s from character %s and targeting %s"),
		*GetName(), *AvatarActor->GetName(), *Target->GetName());

	const auto* AvatarCharacter = Cast<AWolfCharacterBase>(AvatarActor);
	if (IsValid(AvatarCharacter))
	{
		const auto* MoveComp = AvatarCharacter->GetCharacterMovement();
		const auto TargetLocation = Target->GetActorLocation();
		const auto CurrentLocation = AvatarActor->GetActorLocation();

		const auto Direction = (TargetLocation - CurrentLocation).GetSafeNormal2D();
		const auto GoalLocation = TargetLocation - Direction * Period.Range;
		// Moves backwards if too close. Need to review down the line if this behavior should be intended.
		const auto MoveDistance = FVector::Dist(CurrentLocation, GoalLocation);

		const auto MaxSpeed = MoveComp->MaxWalkSpeed;
		const auto Acceleration = MoveComp->MaxAcceleration;
		const auto CurrentVelocity = AvatarActor->GetVelocity().Size();

		Period.Duration = CalculateMovementDuration(MoveDistance, MaxSpeed, Acceleration, CurrentVelocity);
		WOLF_LOG(Log, TEXT("[DURATION TEST] Duration is %.2f seconds."), Period.Duration);
		Period.MoveToDestination = GoalLocation;

		WOLF_LOG(Log, TEXT("%s moving %.2f units. Target's location is %.2f units away."),
			*AvatarActor->GetName(), MoveDistance, FVector::Dist(CurrentLocation, TargetLocation));
		
		auto* MoveTask = UAbilityTask_MoveToLocation::MoveToLocation(
			this,
			TEXT("PresageMoveTo"),
			GoalLocation,
			Period.Duration,
			nullptr,
			nullptr);

		MoveTask->OnTargetLocationReached.AddDynamic(this, &UBaseCombatAbility::OnPeriodCompleted);
		MoveTask->ReadyForActivation();
	}
}

void UBaseCombatAbility::InitializeForSimulation(const TArray<FCombatPeriod>& InSequence)
{
	AbilitySequence = InSequence;
	CurrentPeriodIndex = 0;
	CurrentPeriodStartTime = 0.f;
}

void UBaseCombatAbility::ResolveMoveToDestinations(TArray<FCombatPeriod>& Sequence, const FVector& SourceLocation, const AActor* Target)
{
	if (!Target) return;

	for (auto& Period : Sequence)
	{
		if (Period.Type != EPeriodType::MoveTo) continue;

		const FVector TargetLocation = Target->GetActorLocation();
		const FVector Direction = (TargetLocation - SourceLocation).GetSafeNormal2D();
		Period.MoveToDestination = TargetLocation - Direction * Period.Range;
	}
}

float UBaseCombatAbility::GetPeriodImpactOffset(const FCombatPeriod& Period, const FGameplayTag& HitEventTag)
{
	if (Period.Montage)
	{
		for (const auto& NotifyEvent : Period.Montage->Notifies)
		{
			const auto* HitNotify = Cast<UAnimNotify_Hit>(NotifyEvent.Notify);
			if (HitNotify && HitNotify->EventTag == HitEventTag)
			{
				return NotifyEvent.GetTriggerTime();
			}
		}
		return Period.HitDelay; // No matching notify found on the montage — fall back.
	}
	return Period.HitDelay;
}

FAbilityTimingProfile UBaseCombatAbility::ComputeAbilityTiming(const TArray<FCombatPeriod>& Sequence, const FGameplayTag& HitEventTag)
{
	FAbilityTimingProfile Profile;

	int32 FirstHitPeriod = INDEX_NONE;
	int32 LastHitPeriod = INDEX_NONE;
	float TotalDuration = 0.f;

	for (int32 i = 0; i < Sequence.Num(); ++i)
	{
		if (Sequence[i].HitEffects.Num() > 0)
		{
			if (FirstHitPeriod == INDEX_NONE)
			{
				FirstHitPeriod = i;
			}
			LastHitPeriod = i;
		}
		// GetPeriodDuration substitutes montage play length when a montage exists — same source
		// runtime (AbilityPeriodAdvancer) and the CDO-driven planner must agree on. MoveTo periods
		// still use their authored fallback Duration here (their real duration is distance-dependent
		// and unknowable from the CDO alone) — that is a known, accepted approximation, not a bug.
		TotalDuration += GetPeriodDuration(Sequence[i]);
	}

	Profile.TotalDuration = TotalDuration;

	if (FirstHitPeriod == INDEX_NONE)
	{
		// No hit-capable periods at all (e.g. a pure movement/buff ability) — treat the entire
		// sequence as windup, with no active window and no recovery.
		Profile.WindupDuration = TotalDuration;
		Profile.ActiveWindowStart = TotalDuration;
		Profile.ActiveWindowEnd = TotalDuration;
		Profile.RecoveryDuration = 0.f;
		Profile.FirstImpactTime = TotalDuration;
		return Profile;
	}

	float RunningTime = 0.f;
	for (int32 i = 0; i < FirstHitPeriod; ++i)
	{
		RunningTime += GetPeriodDuration(Sequence[i]);
	}
	Profile.WindupDuration = RunningTime;
	Profile.ActiveWindowStart = RunningTime;
	Profile.FirstImpactTime = RunningTime + GetPeriodImpactOffset(Sequence[FirstHitPeriod], HitEventTag);

	for (int32 i = FirstHitPeriod; i <= LastHitPeriod; ++i)
	{
		RunningTime += GetPeriodDuration(Sequence[i]);
	}
	Profile.ActiveWindowEnd = RunningTime;

	Profile.RecoveryDuration = Profile.TotalDuration - Profile.ActiveWindowEnd;
	return Profile;
}

float UBaseCombatAbility::CalculateMovementDuration(float TotalDistance, float MaxVelocity, float Acceleration, float StartVelocity)
{
	if (Acceleration <= 0.f) return StartVelocity > 0.f ? TotalDistance / StartVelocity : 0.f;
	if (StartVelocity >= MaxVelocity) return TotalDistance / StartVelocity;
	
	const auto TimeToReachMax = (MaxVelocity - StartVelocity) / Acceleration;
	const auto DistanceCoveredDuringAcceleration = StartVelocity * TimeToReachMax + 0.5f * Acceleration * FMath::Square(TimeToReachMax);

	if (DistanceCoveredDuringAcceleration >= TotalDistance) // Never reach MaxVelocity
	{
		return (-StartVelocity + FMath::Sqrt(FMath::Square(StartVelocity) + 2 * Acceleration * TotalDistance)) / Acceleration;
	}

	const auto RemainingDistance = TotalDistance - DistanceCoveredDuringAcceleration;
	const auto TimeAtMax = RemainingDistance / MaxVelocity;
	return TimeToReachMax + TimeAtMax;
}

AActor* UBaseCombatAbility::GetTargetFromBlackboard() const
{
	const auto Info = GetActorInfo();
	if (!Info.AvatarActor.IsValid()) return nullptr;

	const auto* Controller = Info.AvatarActor->GetInstigatorController();
	const AAIController* AIC = Cast<AAIController>(Controller);
	if (AIC)
	{
		if (const auto* BB = AIC->GetBlackboardComponent())
		{
			static const FName TargetKey = TEXT("TargetActor"); // Static FName prevents re-hasing the string every frame/call
			return Cast<AActor>(BB->GetValueAsObject(TargetKey));
		}
	}
	return nullptr;
}

void UBaseCombatAbility::OnPeriodCompleted()
{
	CurrentPeriodIndex++;
	PlayNextPeriod();
}

void UBaseCombatAbility::OnEventReceived(FGameplayEventData EventData)
{
	if (AbilitySequence.IsValidIndex(CurrentPeriodIndex)) { HandleAttackHitEvent(AbilitySequence[CurrentPeriodIndex]); }
}

void UBaseCombatAbility::HandleAttackHitEvent(const FCombatPeriod& CurrentAttackPeriod)
{
	// Override in children
}

bool UBaseCombatAbility::ApplySingleHitEffect(const FCombatHitEffect& Effect, UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC, const FGameplayEffectContextHandle& EffectContext, float AbilityLevel)
{
	if (!Effect.EffectClass) return false;

	const auto EffectSpecHandle = SourceASC->MakeOutgoingSpec(Effect.EffectClass, AbilityLevel, EffectContext);
	if (!EffectSpecHandle.IsValid()) return false;

	const FGameplayTag AmountTag = Effect.DataAmountTag.IsValid()
		? Effect.DataAmountTag
		: FWolfGameplayTags::Get().Data_Amount;

	EffectSpecHandle.Data->SetSetByCallerMagnitude(AmountTag, Effect.Amount.GetValueAtLevel(AbilityLevel));

	if (Effect.bSelfTarget) SourceASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
	else                    SourceASC->ApplyGameplayEffectSpecToTarget(*EffectSpecHandle.Data.Get(), TargetASC);

	return true;
}

bool UBaseCombatAbility::ApplyHitEffects(const FCombatPeriod& Period, AActor* TargetActor, const FHitResult* HitResult)
{
	if (!TargetActor) return false;

	auto* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC) return false;

	auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC) return false;

	auto EffectContext = SourceASC->MakeEffectContext();
	if (HitResult) EffectContext.AddHitResult(*HitResult);

	const auto AbilityLevel = GetAbilityLevel();
	bool bAppliedAny = false;

	for (const FCombatHitEffect& Effect : Period.HitEffects)
	{
		if (ApplySingleHitEffect(Effect, SourceASC, TargetASC, EffectContext, AbilityLevel))
		{
			bAppliedAny = true;
		}
	}

	// Conditional extensions: only apply if the target ASC currently carries every required tag.
	// The reaction window is the duration of whatever GE granted those tags — no separate timer.
	for (const FConditionalHitEffect& Conditional : Period.ConditionalHitEffects)
	{
		if (!TargetASC->HasAllMatchingGameplayTags(Conditional.RequiredTargetTags)) continue;

		if (ApplySingleHitEffect(Conditional.Effect, SourceASC, TargetASC, EffectContext, AbilityLevel))
		{
			WOLF_INFO("Conditional hit effect applied (target had required tags): %s", *TargetActor->GetName());
			bAppliedAny = true;
		}
	}

	// ==============================================================================================================
	// Resource gain hook (ResourceLoop stage 1) — REAL-TIME application path.
	//
	// Gains are a property of the economy, not of individual abilities, so they are computed
	// centrally on every real hit instead of authored per-ability. This site runs once per real
	// period hit (per target); it is mirrored — NOT in ApplySingleHitEffect, which is per-effect
	// and would double-count on multi-effect periods — by the TB execution site
	// (UCombatModeSubsystem::ApplyDueLedgerImpacts, which applies the bake's recorded values)
	// and the prediction site (UWolfPresageComponent::ResolveSimulatedImpact). If PresagePreview
	// stage 3's ledger path is ever routed through this function instead of the subsystem's own
	// loop, this hook is already in the right shape to migrate behind it.
	// ==============================================================================================================
	if (UWorld* AbilityWorld = GetWorld())
	{
		if (auto* CMS = AbilityWorld->GetSubsystem<UCombatModeSubsystem>())
		{
			const auto* SourcePawn = Cast<APawn>(GetAvatarActorFromActorInfo());
			const auto* TargetPawn = Cast<APawn>(TargetActor);
			// TODO(teams): when a team system exists, "player-side dealt/taken hit" replaces "player
			// dealt/taken" here — and at the two mirrored sites — per ResourceLoopStage1.md.
			const bool bPlayerDealtHit = SourcePawn && SourcePawn->IsPlayerControlled();
			const bool bPlayerTookHit = TargetPawn && TargetPawn->IsPlayerControlled();

			// "DamageAmount" = the period's primary (first) hit effect magnitude. Damage is authored
			// positive; the absolute value keeps the rule sign-robust either way.
			float DamageAmount = 0.f;
			if (Period.HitEffects.Num() > 0)
			{
				DamageAmount = FMath::Abs(Period.HitEffects[0].Amount.GetValueAtLevel(AbilityLevel));
			}

			const auto ResourceGain = FWolfResourceRules::ComputeResourceGain(
				bPlayerDealtHit, bPlayerTookHit, DamageAmount, CMS->GetCurrentMode());
			CMS->ApplyResourceGainToPlayer(ResourceGain.FlowDelta, ResourceGain.AdrenalineDelta);
		}
	}

	return bAppliedAny;
}

float UBaseCombatAbility::GetPeriodDuration(const FCombatPeriod& Period)
{
	if (Period.Montage) return Period.Montage->GetPlayLength();
	return Period.Duration;
}

float UBaseCombatAbility::CalculateProjectedImpactTime() const
{
	float TimeAccumulator = 0.f;
	for (const auto& Period : AbilitySequence)
	{
		if (Period.Type != EPeriodType::Attack)
		{
			TimeAccumulator += GetPeriodDuration(Period);
			continue;
		}

		return TimeAccumulator + GetPeriodImpactOffset(Period, HitEventTag);
	}
	return -1.f;
}

bool UBaseCombatAbility::IsInvulnerableAt(float RelativeTime) const
{
	float TimeAccumulator = 0.f;
	for (const auto& Period : AbilitySequence)
	{
		const float PeriodEnd = TimeAccumulator + GetPeriodDuration(Period);
		if (RelativeTime >= TimeAccumulator && RelativeTime < PeriodEnd)
		{
			return Period.Type == EPeriodType::Evasion;
		}
		TimeAccumulator = PeriodEnd;
	}
	return false;
}