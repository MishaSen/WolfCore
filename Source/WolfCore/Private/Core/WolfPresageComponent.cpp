// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/WolfPresageComponent.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/WolfCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Core/WolfAbilityComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "DrawDebugHelpers.h"
#include "Misc/TransactionObjectEvent.h"
#include "Core/WolfSnapshotComponent.h"

UWolfPresageComponent::UWolfPresageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWolfPresageComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<AWolfCharacterBase>(GetOwner());
	if (CharacterOwner)
	{
		CachedASC = Cast<UWolfAbilitySystemComponent>(CharacterOwner->GetAbilitySystemComponent());
		AbilityControl = CharacterOwner->FindComponentByClass<UWolfAbilityComponent>();
	}
}

void UWolfPresageComponent::SimulateTick(float Step)
{
	if (PredictionBuffer.Num() == 0)
	{
		SimulationTransform = CharacterOwner->GetActorTransform();
		SimPeriodTime = 0.f;
	}
	
	SimulatePhysicsStep(Step);
	SimulateAnimationStep(Step);

	SimPeriodTime += Step;

	auto* ActiveAbility = CharacterOwner->GetActiveCombatAbility();
	const bool bSequenceIsActive = IsValid(ActiveAbility) && ActiveAbility->AbilitySequence.IsValidIndex(ActiveAbility->GetCurrentPeriodIndex());
	
	FActorSnapshot FutureFrame;
	if (auto* SnapshotControl = GetSnapshotControl())
	{
		SnapshotControl->SetIsSimulating(true);
		SnapshotControl->SetSimulationTransform(SimulationTransform);
		ISnapshot::Execute_CreateSnapshot(SnapshotControl, FutureFrame);
		SnapshotControl->SetIsSimulating(false);
	}
	
	if (bSequenceIsActive)
	{
		if (IsValid(ActiveAbility))
		{
			const auto& AbilitySequence = ActiveAbility->AbilitySequence;
			const auto CurrentIndex = ActiveAbility->GetCurrentPeriodIndex();
			if (AbilitySequence.IsValidIndex(CurrentIndex))
			{
				auto Duration = 0.f;
				auto& CurrentPeriod = AbilitySequence[CurrentIndex];
				if (CurrentPeriod.Type == EPeriodType::MoveTo)
				{
					Duration = ActiveAbility->CalculateMovementDuration(FVector::Distance(SimulationTransform.GetLocation(), CurrentPeriod.MoveToDestination),
															 GetMoveComp()->MaxWalkSpeed,
															 GetMoveComp()->MaxAcceleration,
															 GetMoveComp()->Velocity.Size());
					WOLF_LOG(Log, TEXT("[CALCULATE MOVEMENT DURATION TEST] Duration is %f."), Duration);
				}
				else Duration = ActiveAbility->GetPeriodDuration(CurrentPeriod);
			
				if (SimPeriodTime >= Duration)
				{
					const auto NextIndex = CurrentIndex + 1;
					ActiveAbility->SetCurrentPeriodIndex(NextIndex);
					SimPeriodTime = 0.f;

					if (AbilitySequence.IsValidIndex(NextIndex))
					{
						WOLF_LOG(Log, TEXT("[SIM] Period %d %s completed. Next: %d %s."),
							CurrentIndex, *UEnum::GetValueAsString(AbilitySequence[CurrentIndex].Type),
							NextIndex, *UEnum::GetValueAsString(AbilitySequence[NextIndex].Type));
					}
					else WOLF_LOG(Log, TEXT("[SIM] Ability Sequence finished at Step %d."), CurrentIndex);
				}
			}
		}
	}
	else
	{
		FutureFrame.ActiveAbility = nullptr;
		FutureFrame.CurrentPeriodIndex = -1;	
	}
	
	WOLF_LOG(Log, TEXT("[STEP %d] Character: %s | Location: %s| Ability: %s | Period: %d | Montage: %s (Pos: %.2f)"),
		PredictionBuffer.Num(),
		*CharacterOwner.GetName(),
		*FutureFrame.Location.ToCompactString(),
		FutureFrame.ActiveAbility.IsValid() ? *FutureFrame.ActiveAbility->GetName() : TEXT("None"),
		FutureFrame.CurrentPeriodIndex,
		FutureFrame.CurrentMontage.IsValid() ? *FutureFrame.CurrentMontage->GetName() : TEXT("None"),
		FutureFrame.MontagePosition);
	PredictionBuffer.Add(FutureFrame); // Remember to clear PredictionBuffer in CombatModeSubsystem
}

void UWolfPresageComponent::ClearPredictionBuffer(float MaxDuration)
{
	const int32 ExpectedFrames = FMath::CeilToInt(MaxDuration * WolfSimConfig::Frequency);
	PredictionBuffer.Empty(ExpectedFrames);
}

const FActorSnapshot* UWolfPresageComponent::GetSnapshotAtTime(float RelativeTime) const
{
	if (PredictionBuffer.Num() == 0) return nullptr;

	const int32 Index = FMath::Clamp(FMath::RoundToInt(RelativeTime * WolfSimConfig::Frequency),
								// Snapshot lookup will drift if the Subsystem uses a variable step or different fixed rate.
							   0,
							   PredictionBuffer.Num() - 1);
	return &PredictionBuffer[Index];
}

void UWolfPresageComponent::SimulatePhysicsStep(float Step)
{
	if (!GetCMS() || !GetMoveComp()) return;

	FVector Destination;
	const auto SimVelocity = GetSimulatedVelocity(Destination);
	
	if (SimVelocity.IsNearlyZero()) return; // Direction might be very small.

	const FVector Start = SimulationTransform.GetLocation();
	FVector Delta = SimVelocity * Step;
	// Not considering starting acceleration, but might not make a difference. Look out for bugs.
	
	if (!Destination.IsZero())
	{
		const float DistanceToTarget = FVector::Dist(Start, Destination);
		if (Delta.Size() > DistanceToTarget) Delta = Delta.GetSafeNormal() * DistanceToTarget;
	}
	const FVector End = Start + Delta;
	ResolveMovementWithCollision(Start, End, Delta);
}

FVector UWolfPresageComponent::GetSimulatedVelocity(FVector& Destination) const
{
	const FVector Velocity = GetMoveComp()->Velocity;
	if (!bIsSimulating && Velocity.IsNearlyZero()) return Velocity;
	
	if (const auto* Ability = CharacterOwner->GetActiveCombatAbility())
	{
		const auto& Sequence = Ability->AbilitySequence;
		const auto Index = Ability->GetCurrentPeriodIndex();
		if (Sequence.IsValidIndex(Index) && Sequence[Index].Type == EPeriodType::MoveTo)
		{
			Destination = Sequence[Index].MoveToDestination;
			const FVector ToDestination = Destination - SimulationTransform.GetLocation();
			return ToDestination.GetSafeNormal() * GetMoveComp()->MaxWalkSpeed;
		}
	}
	return Velocity;
}

void UWolfPresageComponent::ResolveMovementWithCollision(const FVector& Start, const FVector& End, FVector& Delta)
{
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);
	
	const FQuat Rotation = SimulationTransform.GetRotation();
	const FCollisionShape Shape = CharacterOwner->GetCapsuleComponent()->GetCollisionShape();
	
	FHitResult Hit(1.f);
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, Rotation,ECC_Pawn, Shape, Params);
	if (!bHit) { SimulationTransform.SetLocation(End); return; }

	const FVector RemainingDelta = Delta * (1.f - Hit.Time);
	const FVector SlideDelta = FVector::VectorPlaneProject(RemainingDelta, Hit.Normal);
	if (SlideDelta.IsNearlyZero()) { SimulationTransform.SetLocation(Hit.Location); return; }

	FHitResult SlideHit;
	const FVector SlideEnd = Hit.Location + SlideDelta;
	const bool bSlideHit = GetWorld()->
		SweepSingleByChannel(SlideHit,Hit.Location, SlideEnd, Rotation,ECC_Pawn, Shape, Params);
	
	SimulationTransform.SetLocation(bSlideHit ? SlideHit.Location : SlideEnd);
}

void UWolfPresageComponent::SimulateAnimationStep(float DeltaTime)
{
	auto* AnimInst = GetAnimInst();
	if (!IsValid(AnimInst)) return;
	
	const auto* CurrentMontage = GetCurrentMontage();
	if (!IsValid(CurrentMontage)) return;

	const auto CurrentPos = AnimInst->Montage_GetPosition(CurrentMontage);
	const auto NewPos = CurrentPos + DeltaTime;

	AnimInst->Montage_SetPosition(CurrentMontage, NewPos);

	if (CurrentMontage->HasRootMotion())
	{
		const auto RootMotionDelta = CurrentMontage->ExtractRootMotionFromRange(CurrentPos,NewPos, FAnimExtractContext());
		const auto WorldDelta = GetOwnerRotation().RotateVector(RootMotionDelta.GetLocation());

		if (bIsSimulating) SimulationTransform.AddToTranslation(WorldDelta);
		else WOLF_WARN(TEXT("SimulateAnimStep() activating during outside of Simulation."));
	}

	if (NewPos >= CurrentMontage->GetPlayLength())
	{
		AnimInst->Montage_Stop(0.1f, CurrentMontage); // If seeing t-poses, set to idle state
	}
}

FVector UWolfPresageComponent::GetOwnerLocation() const { return CharacterOwner ? CharacterOwner->GetActorLocation() : FVector::ZeroVector; }
FRotator UWolfPresageComponent::GetOwnerRotation() const { return CharacterOwner ? CharacterOwner->GetActorRotation() : FRotator::ZeroRotator; }
FVector UWolfPresageComponent::GetSimLocation() const { return bIsSimulating ? SimulationTransform.GetLocation() : GetOwnerLocation(); }
FRotator UWolfPresageComponent::GetSimRotation() const { return bIsSimulating ? SimulationTransform.GetRotation().Rotator() : GetOwnerRotation(); }

UCharacterMovementComponent* UWolfPresageComponent::GetMoveComp() const { return CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr; }
UWolfAbilitySystemComponent* UWolfPresageComponent::GetASC() const { return CachedASC; }
UAnimInstance* UWolfPresageComponent::GetAnimInst() const { return CharacterOwner ? CharacterOwner->GetAnimInst() : nullptr; }
UAnimMontage* UWolfPresageComponent::GetCurrentMontage() const { return GetAnimInst() ? GetAnimInst()->GetCurrentActiveMontage() : nullptr; }
UCombatModeSubsystem* UWolfPresageComponent::GetCMS() const { return CharacterOwner ? CharacterOwner->GetCMS() : nullptr; }

UWolfSnapshotComponent* UWolfPresageComponent::GetSnapshotControl() const
{
	return CharacterOwner ? CharacterOwner->GetSnapshotComponent() : nullptr;
}
