// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Presage/PresageSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/WolfAttributeSet.h"
#include "Algo/ForEach.h"
#include "Animation/AnimInstance.h"
#include "Character/WolfCharacterBase.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/CombatModeSubsystem.h"
#include "TimerManager.h"
#include "WolfCore/Public/Presage/ActorSnapshot.h"
#include "WolfCore/Public/Presage/PresageAbilityRequest.h"

void UPresageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	WolfTags = &FWolfGameplayTags::Get();
}

void UPresageSubsystem::Deinitialize()
{
	Super::Deinitialize();
	StopLoop();
}

UPresageSubsystem* UPresageSubsystem::Get(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UPresageSubsystem>();
}

void UPresageSubsystem::Tick(float DeltaTime)
{
	if (!bLoopActive)
	{
		return;
	}

	AccumulatedTime += DeltaTime;
	if (AccumulatedTime >= FlowTime)
	{
		AccumulatedTime -= FlowTime;
		OnFlowTimerTick();
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// Process queued abilities
	for (int32 i = AbilityQueue.Num() - 1; i >= 0; --i)
	{
		if (const FPresageAbilityRequest& Request = AbilityQueue[i]; Request.GetScheduledTime() <= CurrentTime)
		{
			if (auto ASC = Request.GetOwnerASC())
			{
				// TODO: Send request through interface to character for activation.
			}
			AbilityQueue.RemoveAt(i);
		}
	}
}

void UPresageSubsystem::StartLoop()
{
	if (bLoopActive)
	{
		return;
	}

	bLoopActive = true;
	AccumulatedTime = 0.f;

	CharacterSnapshot(); // 1. Save the present
	UpdateTimelinePrediction(); // 2. Calculate the future
	// 3. TODO: Update UI to read 'CurrentPredictedTimeline' and draw icons
}

void UPresageSubsystem::StopLoop()
{
	if (!bLoopActive)
	{
		return;
	}

	bLoopActive = false;
	AccumulatedTime = 0.f;
}

void UPresageSubsystem::BakeSimulation()
{
	VisualTracks.Empty();

	const int32 NumSteps = FMath::CeilToInt(FlowTime / PredictionTimeStep);

	auto ProcessParticipant = [this, NumSteps](const TWeakObjectPtr<AWolfCharacterBase>& Character)
	{
		auto* SimulatedCharacter = Character.Get();
		if (!IsValid(SimulatedCharacter)) return;

		const auto* Mesh = SimulatedCharacter->GetMesh();
		const auto* AnimInst = Mesh ? Mesh->GetAnimInstance() : nullptr;

		// TODO: If nothing is playing yet, should we check Ability Queue for future planned abilities (e.g., for TB Characters)?
		auto* Montage = AnimInst ? AnimInst->GetCurrentActiveMontage() : nullptr;
		const float MontageStartPosition = AnimInst ? AnimInst->Montage_GetPosition(Montage) : 0.f;
		const float MontagePlayRate = AnimInst && Montage ? AnimInst->Montage_GetPlayRate(Montage) : 1.f;

		auto& Track = VisualTracks.FindOrAdd(SimulatedCharacter);
		Track.Frames.Reset();
		Track.Frames.Reserve(NumSteps);

		for (float SimTime = 0.f; SimTime <= FlowTime; SimTime += PredictionTimeStep)
		{
			const auto ProjectedTransform = SimulatedCharacter->GetProjectedTransform(SimTime);

			auto& NewFrame = Track.Frames.AddDefaulted_GetRef();
			NewFrame.Timestamp = SimTime;
			NewFrame.Location = ProjectedTransform.GetLocation();
			NewFrame.Rotation = ProjectedTransform.Rotator();
			NewFrame.ActiveMontage = Montage;
			NewFrame.MontagePosition = Montage ? MontageStartPosition + SimTime * MontagePlayRate : 0.f;
		}
	};

	Algo::ForEach(RTParticipants, ProcessParticipant);
	Algo::ForEach(TBParticipants, ProcessParticipant);
}

void UPresageSubsystem::ScrubToTime(float Time)
{
	for (auto& [Character, Track] : VisualTracks)
	{
		if (!Character || Track.Frames.Num() == 0) continue;

		auto FrameIndex = FMath::RoundToInt(Time / PredictionTimeStep);
		FrameIndex = FMath::Clamp(FrameIndex, 0, Track.Frames.Num() - 1);
		if (!Track.Frames.IsValidIndex(FrameIndex)) continue;

		const auto& Frame = Track.Frames[FrameIndex];

		if (auto* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}

		Character->SetActorLocationAndRotation(Frame.Location, Frame.Rotation);

		auto* AnimInst = Character->GetMesh()->GetAnimInstance();
		auto* Montage = Frame.ActiveMontage.Get();

		if (AnimInst && Montage)
		{
			if (!AnimInst->Montage_IsPlaying(Montage))
			{
				AnimInst->Montage_Play(Montage, 0.f);
			}
			AnimInst->Montage_SetPosition(Montage, Frame.MontagePosition);
		}
	}
}


void UPresageSubsystem::OnFlowTimerTick()
{
	RevertCharacterStates();
}

void UPresageSubsystem::QueueAbilityRequest(const FPresageAbilityRequest& Request)
{
	AbilityQueue.Add(Request);
	StartLoop();
}

void UPresageSubsystem::CharacterSnapshot()
{
	OriginalCharacterStates.Empty();

	const auto* CMS = GetWorld()->GetSubsystem<UCombatModeSubsystem>();
	if (!CMS) return;

	for (const auto& Combatant : CMS->GetTrackedCombatants())
	{
		if (auto* Actor = Combatant.Get())
		{
			FActorSnapshot NewSnap;
			Execute_CreateSnapshot(Actor, NewSnap);
			OriginalCharacterStates.Add(NewSnap);
		}
	}
}

void UPresageSubsystem::RevertCharacterStates()
{
	for (const auto& Snapshot : OriginalCharacterStates)
	{
		if (!Snapshot.ActorRef.Get()) return;
		Execute_RestoreSnapshot(Snapshot.ActorRef.Get(), Snapshot);
	}
}

void UPresageSubsystem::UpdateTimelinePrediction()
{
	CurrentPredictedTimeline.Empty();

	BakeSimulation();

	TArray<FPresageTimelineEvent> PotentialEvents;

	GatherRTEvents(PotentialEvents);
	GatherTBEvents(PotentialEvents);
	OrganizeEventsByTime(PotentialEvents);

	CurrentPredictedTimeline = PotentialEvents;
}

bool UPresageSubsystem::CheckFutureCollision(const AWolfCharacterBase* Attacker, const AWolfCharacterBase* Victim,
                                             float FutureTime)
{
	const auto AttackerTransform = Attacker->GetProjectedTransform(FutureTime);

	float Radius, HalfHeight;
	Victim->GetPresageCollisionDimensions(Radius, HalfHeight);

	const FVector AttackLocation = AttackerTransform.GetLocation() + AttackerTransform.GetRotation().GetForwardVector()
		* 100.f;
	const float Distance = FVector::Dist(AttackLocation, Victim->GetActorLocation());

	return Distance < Radius + 50.f;
}

float UPresageSubsystem::CalculateImpactFromSequence(const TArray<FCombatPeriod>& Sequence)
{
	auto TimeAccumulator = 0.f;
	for (const auto& Period : Sequence)
	{
		if (Period.Type == EPeriodType::Attack) return TimeAccumulator;

		TimeAccumulator += Period.Duration;
	}
	return -1.f;
}

void UPresageSubsystem::SetParticipantMode(AWolfCharacterBase* Character, bool bIsTurnBased)
{
	if (!IsValid(Character)) return;

	RTParticipants.RemoveSwap(Character); // Ensure no dupes
	TBParticipants.RemoveSwap(Character); // RemoveSwap() messes with array order but faster than Remove()

	if (bIsTurnBased) TBParticipants.Add(Character);
	else RTParticipants.Add(Character);
}

void UPresageSubsystem::RemoveParticipant(AWolfCharacterBase* Character)
{
	RTParticipants.Remove(Character);
	TBParticipants.Remove(Character);
}

void UPresageSubsystem::GatherRTEvents(TArray<FPresageTimelineEvent>& Events)
{
	for (auto& RTAttacker : RTParticipants)
	{
		auto* RTCharacter = RTAttacker.Get();
		const auto* ActiveAbility = Cast<UBaseCombatAbility>(RTCharacter ? RTCharacter->GetActiveCombatAbility() : nullptr);

		if (!ActiveAbility) continue;

		// TODO: Implement method to calculate elapsed time. Impact time will be different if ability was already active.
		const auto ImpactTime = ActiveAbility->CalculateProjectedImpactTime();
		if (ImpactTime <= 0.f || ImpactTime > FlowTime) continue;

		for (auto& TBAttacker : TBParticipants)
		{
			auto* TBCharacter = TBAttacker.Get();
			if (TBCharacter && CheckFutureCollision(RTCharacter, TBCharacter, ImpactTime))
			{
				Events.Emplace(FPresageTimelineEvent(RTCharacter, TBCharacter, ImpactTime, WolfTags->Result_Hit));
			}
		}
	}
}

void UPresageSubsystem::GatherTBEvents(TArray<FPresageTimelineEvent>& Events)
{
	for (const auto& Request : AbilityQueue)
	{
		const auto* AbilityCDO = Request.GetAbilityCDO();
		if (!AbilityCDO) continue;

		auto* TBAttacker = Cast<AWolfCharacterBase>(Request.GetOwnerASC()->GetAvatarActor());
		if (!TBAttacker) continue;

		auto AbilitySequenceImpactTime = CalculateImpactFromSequence(Request.GetAbilitySequence());
		if (AbilitySequenceImpactTime < 0.f) continue;

		auto PresageSequenceImpactTime = AbilitySequenceImpactTime + Request.GetScheduledTime();

		for (auto& TargetActor : Request.GetTargets())
		{
			auto* Target = Cast<AWolfCharacterBase>(TargetActor.Get());
			if (!Target) continue;

			Events.Add(FPresageTimelineEvent(TBAttacker, Target, PresageSequenceImpactTime, WolfTags->Result_Hit));
		}
	}
}

void UPresageSubsystem::OrganizeEventsByTime(TArray<FPresageTimelineEvent>& Events)
{
	Events.Sort([](const FPresageTimelineEvent& A, const FPresageTimelineEvent& B)
	{
		return A.Time < B.Time;
	});

	TSet<AActor*> InterruptedActors;
	TArray<FPresageTimelineEvent> TimeSortedEvents;

	for (auto& Event : Events)
	{
		if (InterruptedActors.Contains(Event.Attacker)) continue;

		const auto* Victim = Cast<AWolfCharacterBase>(Event.Victim);
		bool bIsInvulnerable = false;

		if (Victim)
		{
			bIsInvulnerable = Victim->IsInvulnerableAt(Event.Time);
		}

		if (bIsInvulnerable)
		{
			Event.ResultTag = WolfTags->Result_Dodge;
		}
		else
		{
			Event.ResultTag = WolfTags->Result_Hit;
			InterruptedActors.Add(Event.Victim);
		}

		TimeSortedEvents.Add(Event);
	}
	Events = TimeSortedEvents;
}
