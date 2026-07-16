// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/WolfPresageComponent.h"

#include "Abilities/AbilityPeriodAdvancer.h"
#include "Abilities/BaseCombatAbility.h"
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
#include "Systems/CombatModeSubsystem.h"

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
	if (!CharacterOwner) return;

	SimElapsedTime += Step;
	TryActivateInjectedAbility();

	SimulatePhysicsStep(Step);
	SimulateAnimationStep(Step);

	// Advance period BEFORE snapshot capture so that both CreateSnapshot and
	// CurrentPeriodIndex assignment reflect the same post-advancement state.
	SimPeriodTime += Step;
	if (auto* ActiveAbility = GetActiveSimulationAbility())
	{
		const int32 PeriodBeforeAdvance = ActiveAbility->GetCurrentPeriodIndex();

		SimPeriodTime = FAbilityPeriodAdvancer::AdvancePeriod(
			ActiveAbility,
			SimPeriodTime,
			GetMoveComp()->MaxWalkSpeed,
			GetMoveComp()->MaxAcceleration,
			GetMoveComp()->Velocity.Size(),
			CharacterOwner->GetActorLocation()
		);
	}

	FActorSnapshot FutureFrame;
	if (auto* SnapshotControl = GetSnapshotControl())
	{
		ISnapshot::Execute_CreateSnapshot(SnapshotControl, FutureFrame);
	}

	FutureFrame.ActiveAbility = GetActiveSimulationAbility();
	FutureFrame.CurrentPeriodIndex = FutureFrame.ActiveAbility.IsValid()
		? FutureFrame.ActiveAbility->GetCurrentPeriodIndex()
		: -1;

	/*WOLF_LOG(Log, TEXT("[STEP %d] Character: %s | Location: %s| Ability: %s | Period: %d | Montage: %s (Pos: %.2f)"),
		PredictionBuffer.Num(),
		*CharacterOwner.GetName(),
		*FutureFrame.Location.ToCompactString(),
		FutureFrame.ActiveAbility.IsValid() ? *FutureFrame.ActiveAbility->GetName() : TEXT("None"),
		FutureFrame.CurrentPeriodIndex,
		FutureFrame.CurrentMontage.IsValid() ? *FutureFrame.CurrentMontage->GetName() : TEXT("None"),
		FutureFrame.MontagePosition);*/
	PredictionBuffer.Add(FutureFrame); // Remember to clear PredictionBuffer in CombatModeSubsystem
}

void UWolfPresageComponent::ClearPredictionBuffer(float MaxDuration)
{
	const float StepSize = GetCMS() ? GetCMS()->GetBakedStepSize() : 0.1f; // Fallback only; GetCMS() should not normally be null.
	const int32 ExpectedFrames = FMath::CeilToInt(MaxDuration / StepSize);
	PredictionBuffer.Empty(ExpectedFrames);
}

const FActorSnapshot* UWolfPresageComponent::GetSnapshotAtTime(float RelativeTime) const
{
	if (PredictionBuffer.Num() == 0) return nullptr;

	// Read the step size from the subsystem that owns the bake-time invariant.
	// This ensures index math uses the actual step size used during ExecuteFutureBake,
	// not a compile-time constant that could drift out of sync if the simulator changes.
	const float StepSize = GetCMS() ? GetCMS()->GetBakedStepSize() : 0.1f; // Fallback only; GetCMS() should not normally be null.
	const int32 Index = FMath::Clamp(
		FMath::RoundToInt(RelativeTime / StepSize),
		0,
		PredictionBuffer.Num() - 1);
	return &PredictionBuffer[Index];
}

void UWolfPresageComponent::SimulatePhysicsStep(float Step)
{
	if (!GetCMS() || !GetMoveComp()) return;

	FVector Destination;
	const auto SimVelocity = GetSimulatedVelocity(Destination);
	
	if (SimVelocity.IsNearlyZero()) return; // Return early if simulated velocity is negligible.

	const FVector Start = CharacterOwner->GetActorLocation();
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
	
	if (const auto* Ability = GetActiveSimulationAbility())
	{
		const auto& Sequence = Ability->GetAbilitySequence();
		const auto Index = Ability->GetCurrentPeriodIndex();
		if (Sequence.IsValidIndex(Index) && Sequence[Index].Type == EPeriodType::MoveTo)
		{
			Destination = Sequence[Index].MoveToDestination;
			const FVector ToDestination = Destination - CharacterOwner->GetActorLocation();
			return ToDestination.GetSafeNormal() * GetMoveComp()->MaxWalkSpeed;
		}
	}
	return Velocity;
}

void UWolfPresageComponent::ResolveMovementWithCollision(const FVector& Start, const FVector& End, FVector& Delta)
{
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);
	
	const FQuat Rotation = CharacterOwner->GetActorQuat();
	const FCollisionShape Shape = CharacterOwner->GetCapsuleComponent()->GetCollisionShape();
	
	FHitResult Hit(1.f);
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, Rotation,ECC_Pawn, Shape, Params);
	if (!bHit) { CharacterOwner->SetActorLocation(End, false, nullptr, ETeleportType::TeleportPhysics); return; }

	const FVector RemainingDelta = Delta * (1.f - Hit.Time);
	const FVector SlideDelta = FVector::VectorPlaneProject(RemainingDelta, Hit.Normal);
	if (SlideDelta.IsNearlyZero()) { CharacterOwner->SetActorLocation(Hit.Location, false, nullptr, ETeleportType::TeleportPhysics); return; }

	FHitResult SlideHit;
	const FVector SlideEnd = Hit.Location + SlideDelta;
	const bool bSlideHit = GetWorld()->
		SweepSingleByChannel(SlideHit,Hit.Location, SlideEnd, Rotation,ECC_Pawn, Shape, Params);
	
	CharacterOwner->SetActorLocation(bSlideHit ? SlideHit.Location : SlideEnd, false, nullptr, ETeleportType::TeleportPhysics);
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

		CharacterOwner->AddActorWorldOffset(WorldDelta, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (NewPos >= CurrentMontage->GetPlayLength())
	{
		AnimInst->Montage_Stop(0.1f, CurrentMontage); // If seeing t-poses, set to idle state
	}
}

FVector UWolfPresageComponent::GetOwnerLocation() const { return CharacterOwner ? CharacterOwner->GetActorLocation() : FVector::ZeroVector; }
FRotator UWolfPresageComponent::GetOwnerRotation() const { return CharacterOwner ? CharacterOwner->GetActorRotation() : FRotator::ZeroRotator; }

UCharacterMovementComponent* UWolfPresageComponent::GetMoveComp() const { return CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr; }
UWolfAbilitySystemComponent* UWolfPresageComponent::GetASC() const { return CachedASC; }
UAnimInstance* UWolfPresageComponent::GetAnimInst() const { return CharacterOwner ? CharacterOwner->GetAnimInst() : nullptr; }
UAnimMontage* UWolfPresageComponent::GetCurrentMontage() const { return GetAnimInst() ? GetAnimInst()->GetCurrentActiveMontage() : nullptr; }
UCombatModeSubsystem* UWolfPresageComponent::GetCMS() const { return CharacterOwner ? CharacterOwner->GetCMS() : nullptr; }

UWolfSnapshotComponent* UWolfPresageComponent::GetSnapshotControl() const
{
	return CharacterOwner ? CharacterOwner->GetSnapshotComponent() : nullptr;
}

void UWolfPresageComponent::SetInjectedAbilityRequest(const FPresageAbilityRequest& Request)
{
	InjectedAbilityRequest = Request;
}

void UWolfPresageComponent::ClearInjectedAbilityRequest()
{
	InjectedAbilityRequest = FPresageAbilityRequest();
}

void UWolfPresageComponent::BeginSimulation()
{
	SimElapsedTime = 0.f;
	SimPeriodTime = 0.f;
	bSimulatedAbilityActive = false;
	LastSimulatedPeriodIndex = -1;
	SimulatedAbility = nullptr;
}

void UWolfPresageComponent::EndSimulation()
{
	SimulatedAbility = nullptr;
	bSimulatedAbilityActive = false;
	LastSimulatedPeriodIndex = -1;
	ClearInjectedAbilityRequest();
}

UBaseCombatAbility* UWolfPresageComponent::GetActiveSimulationAbility() const
{
	if (!CharacterOwner) return nullptr;

	if (auto* RealAbility = CharacterOwner->GetActiveCombatAbility())
	{
		if (RealAbility->GetAbilitySequence().IsValidIndex(RealAbility->GetCurrentPeriodIndex()))
		{
			return RealAbility;
		}
		// RealAbility's simulated sequence has run its course for this bake — fall through so an
		// injected ability (if any) can take over instead of being permanently blocked.
	}

	if (bSimulatedAbilityActive && SimulatedAbility)
	{
		return SimulatedAbility;
	}

	return nullptr;
}

void UWolfPresageComponent::TryActivateInjectedAbility()
{
	if (bSimulatedAbilityActive || !HasInjectedAbilityRequest()) return;
	if (SimElapsedTime < InjectedAbilityRequest.GetScheduledTime()) return;

	SimulatedAbility = NewObject<UBaseCombatAbility>(
		this,
		InjectedAbilityRequest.AbilityClass,
		NAME_None,
		RF_Transient);

	TArray<FCombatPeriod> Sequence = InjectedAbilityRequest.GetAbilitySequence();

	AActor* Target = nullptr;
	for (const auto& WeakTarget : InjectedAbilityRequest.GetTargets())
	{
		if (WeakTarget.IsValid())
		{
			Target = WeakTarget.Get();
			break;
		}
	}

	UBaseCombatAbility::ResolveMoveToDestinations(
		Sequence,
		CharacterOwner->GetActorLocation(),
		Target);

	SimulatedAbility->InitializeForSimulation(Sequence);
	bSimulatedAbilityActive = true;
	LastSimulatedPeriodIndex = 0;
	SyncSimulationMontage(SimulatedAbility);

	WOLF_LOG(Log, TEXT("[PRESAGE] Injected %s at t=%.2fs for %s"),
		*InjectedAbilityRequest.AbilityClass->GetName(),
		SimElapsedTime,
		*CharacterOwner->GetName());
}

void UWolfPresageComponent::SyncSimulationMontage(UBaseCombatAbility* Ability)
{
	if (!Ability) return;

	const auto& Sequence = Ability->GetAbilitySequence();
	const int32 Index = Ability->GetCurrentPeriodIndex();
	if (!Sequence.IsValidIndex(Index)) return;

	const auto& Period = Sequence[Index];
	if (!IsValid(Period.Montage)) return;

	if (auto* AnimInst = GetAnimInst())
	{
		AnimInst->Montage_Play(Period.Montage);
		AnimInst->Montage_SetPosition(Period.Montage, 0.f);
	}
}
