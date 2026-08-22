// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/WolfSnapshotComponent.h"

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
#include "AbilitySystem/WolfAbilitySystemComponent.h"

UWolfSnapshotComponent::UWolfSnapshotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWolfSnapshotComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<AWolfCharacterBase>(GetOwner());
	if (OwnerCharacter)
	{
		CachedASC = Cast<UWolfAbilitySystemComponent>(OwnerCharacter->GetAbilitySystemComponent());
		AbilityControl = OwnerCharacter->FindComponentByClass<UWolfAbilityComponent>();
		CachedMoveComp = OwnerCharacter->GetCharacterMovement();
	}
}

void UWolfSnapshotComponent::CreateSnapshot_Implementation(FActorSnapshot& OutSnapshot)
{
	if (!OwnerCharacter) return;

	OutSnapshot.ActorRef = OwnerCharacter.Get();
	SnapshotPhysics(OutSnapshot);
	SnapshotGAS(OutSnapshot);
	SnapshotAnim(OutSnapshot);
}

void UWolfSnapshotComponent::RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot)
{
	bIsRestoringSnapshot = true;

	RestorePhysics(Snapshot);
	RestoreGAS(Snapshot);
	RestoreAnim(Snapshot);

	bIsRestoringSnapshot = false;
}

void UWolfSnapshotComponent::SnapshotPhysics(FActorSnapshot& Snapshot) const
{
	Snapshot.Location = GetOwnerLocation();
	Snapshot.Rotation = GetOwnerRotation();
	Snapshot.Velocity = OwnerCharacter->GetVelocity();
	Snapshot.MovementMode = GetMoveComp()->MovementMode;
	Snapshot.CustomMovementMode = GetMoveComp()->CustomMovementMode;

	const auto* Controller = OwnerCharacter->GetController();
	if (!IsValid(Controller)) return;

	if (const auto* ActiveAbility = OwnerCharacter->GetActiveCombatAbility())
	{
		const auto Index = ActiveAbility->GetCurrentPeriodIndex();
		const auto& Sequence = ActiveAbility->GetAbilitySequence();
		if (!Sequence.IsValidIndex(Index)) return;

		const auto& CurrentPeriod = Sequence[Index];
		if (CurrentPeriod.Type == EPeriodType::MoveTo)
		{
			Snapshot.Destination = CurrentPeriod.MoveToDestination;
			Snapshot.bIsMoving = true;

			DrawDebugSphere(GetWorld(), Snapshot.Destination, 25.f, 12, FColor::Red, false, 5.f);
			DrawDebugLine(GetWorld(), GetOwnerLocation(), Snapshot.Destination, FColor::Red, false, 5.f, 0, 2.f);
		}
	}
	else
	{
		Snapshot.bIsMoving = false;
		Snapshot.Destination = FVector::ZeroVector;
	}

	if (const auto* AICont = Cast<AAIController>(Controller);
		const auto* BB = AICont->GetBlackboardComponent())
	{
		Snapshot.TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
		if (Snapshot.TargetActor.IsValid())
		{
			const auto TargetBox = Snapshot.TargetActor->GetRootComponent()->Bounds.GetBox();
			if (!TargetBox.IsValid) return;
			DrawDebugBox(GetWorld(), TargetBox.GetCenter(), TargetBox.GetExtent(), FColor::Orange, false, 5.f, 0, 3.f);
		}
	}

	WOLF_LOG(Log, TEXT("[Snapshot: %d] Character %s has Path Destination %s targeting %s. IsMoving = %s."),
	         SnapshotIndex,
	         *OwnerCharacter->GetName(),
	         *Snapshot.Destination.ToCompactString(),
	         Snapshot.TargetActor.IsValid() ? *Snapshot.TargetActor->GetName() : TEXT("None"),
	         Snapshot.bIsMoving ? TEXT ("True") : TEXT("False"));
	SnapshotIndex++;
}

void UWolfSnapshotComponent::RestorePhysics(const FActorSnapshot& Snapshot)
{
	if (!IsValid(OwnerCharacter)) return;

	WOLF_LOG(Log, TEXT("Character %s has location %s and rotation %s."),
	         *OwnerCharacter.GetName(),
	         *OwnerCharacter->GetActorLocation().ToCompactString(),
	         *OwnerCharacter->GetActorRotation().ToCompactString());

	OwnerCharacter->SetActorLocationAndRotation
	(
		Snapshot.Location,
		Snapshot.Rotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	WOLF_LOG(Log, TEXT("Set Character %s to location %s and rotation %s. He now has location %s and rotation %s."),
	         *OwnerCharacter.GetName(),
	         *Snapshot.Location.ToCompactString(),
	         *Snapshot.Rotation.ToCompactString(),
	         *OwnerCharacter->GetActorLocation().ToCompactString(),
	         *OwnerCharacter->GetActorRotation().ToCompactString());

	DrawDebugSphere(GetWorld(), OwnerCharacter->GetActorLocation(), 25.f, 12, FColor::Green, false, 5.f);

	auto* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (IsValid(Capsule))
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	}

	if (IsValid(GetMoveComp()))
	{
		// Tick-state ownership belongs to the bake lifecycle (SetupCombatantSimulation disables,
		// CleanupCombatantSimulation re-enables) — a restore that force-enables ticks mid-bake
		// would fight the simulator's freeze. Only movement mode is restored here.
		GetMoveComp()->SetMovementMode(Snapshot.MovementMode, Snapshot.CustomMovementMode);
		GetMoveComp()->Velocity = Snapshot.Velocity;
		GetMoveComp()->UpdateComponentVelocity();
	}

	auto* PrimitiveComp = Cast<UPrimitiveComponent>(OwnerCharacter->GetRootComponent());
	if (IsValid(PrimitiveComp))
	{
		PrimitiveComp->SetPhysicsLinearVelocity(Snapshot.Velocity);
	}
}

void UWolfSnapshotComponent::SnapshotAnim(FActorSnapshot& Snapshot) const
{
	if (auto* CurrentMontage = GetCurrentMontage())
	{
		Snapshot.CurrentMontage = CurrentMontage;
		Snapshot.MontagePosition = GetAnimInst()->Montage_GetPosition(CurrentMontage);
	}
}

void UWolfSnapshotComponent::RestoreAnim(const FActorSnapshot& Snapshot)
{
	if (auto* AnimInst = GetAnimInst())
	{
		AnimInst->StopAllMontages(0.f);
		if (!Snapshot.CurrentMontage.IsValid()) return;

		AnimInst->Montage_Play(Snapshot.CurrentMontage.Get(), 1.f);
		AnimInst->Montage_SetPosition(Snapshot.CurrentMontage.Get(), Snapshot.MontagePosition);
	}
}

void UWolfSnapshotComponent::SnapshotGAS(FActorSnapshot& Snapshot) const
{
	if (!IsValid(CachedASC) || !IsValid(AbilityControl)) return;

	const auto& Attributes = AbilityControl->GetCachedAttributes();

	Snapshot.AttributeValues.Empty(Attributes.Num());
	for (const auto& Attribute : Attributes)
	{
		Snapshot.AttributeValues.Add(CachedASC->GetNumericAttribute(Attribute));
	}

	// ActiveEffects is no longer populated (PresagePreviewStage3): preview never applies real GEs,
	// so there is nothing to round-trip, and a restore-by-reapplication path can never be provably
	// exact (durations, periodic-tick phase, and stack state don't survive reconstruction). See
	// FActorSnapshot::ActiveEffects's deprecation comment.

	auto* ActiveAbility = OwnerCharacter->GetActiveCombatAbility();
	const auto CurrentIndex = ActiveAbility ? ActiveAbility->GetCurrentPeriodIndex() : -1;
	if (IsValid(ActiveAbility) && ActiveAbility->GetAbilitySequence().IsValidIndex(CurrentIndex))
	{
		Snapshot.ActiveAbility = ActiveAbility;
		Snapshot.CurrentPeriodIndex = CurrentIndex;
	}
}

void UWolfSnapshotComponent::RestoreGAS(const FActorSnapshot& Snapshot)
{
	if (!IsValid(CachedASC) || !IsValid(AbilityControl) || !AbilityControl->GetStatConfig()) return;

	// Effect-removal/reapplication (the Effect_Combat filter + Snapshot.ActiveEffects loop) is
	// gone as of PresagePreviewStage3 — SnapshotGAS no longer populates ActiveEffects, and preview
	// never applies real GEs at all, so there was nothing correct left for this block to restore.
	// This resolves the old capture/restore asymmetry by removing the asymmetric machinery, not
	// by symmetrizing its filter — see PresagePreviewStage1's contract for the rationale.
	if (RestoreDetail == ERestoreDetail::Full)
	{
		const auto& Attributes = AbilityControl->GetCachedAttributes();
		CachedASC->SetTagMapCount(FWolfGameplayTags::Get().InputState_Dead, 0);

		for (int32 StatIndex = 0; StatIndex < Attributes.Num(); ++StatIndex)
		{
			if (!Snapshot.AttributeValues.IsValidIndex(StatIndex)) break;

			const auto& Attribute = Attributes[StatIndex];
			const auto SavedValue = Snapshot.AttributeValues[StatIndex];

			if (!FMath::IsNearlyEqual(CachedASC->GetNumericAttribute(Attribute), SavedValue))
			{
				CachedASC->SetNumericAttributeBase(Attribute, SavedValue);
			}
		}
	}
	// Presentational restore (TB Executing playback) skips attributes and the dead-tag reset
	// entirely — real GameplayEffect application (UCombatModeSubsystem::ApplyDueLedgerImpacts) is
	// the source of truth for attributes during Executing, applied separately, once, through the
	// real GAS path. Ability/period-index/montage restore below always runs regardless of detail.

	if (Snapshot.ActiveAbility.IsValid())
	{
		auto* Ability = Snapshot.ActiveAbility.Get();
		Ability->SetCurrentPeriodIndex(Snapshot.CurrentPeriodIndex);

		const auto Montage = Snapshot.CurrentMontage.Get();
		if (!IsValid(Montage) || !IsValid(GetAnimInst())) return;

		if (GetAnimInst()->Montage_IsPlaying(Montage)) GetAnimInst()->Montage_SetPosition(
			Montage, Snapshot.MontagePosition);
		else
		{
			AbilityControl->GetWolfASC()->PlayMontage(
				Ability,
				Ability->GetCurrentActivationInfo(),
				Montage,
				1.f,
				NAME_None,
				Snapshot.MontagePosition);
		}
	}
}

FVector UWolfSnapshotComponent::GetOwnerLocation() const
{
	return OwnerCharacter ? OwnerCharacter->GetActorLocation() : FVector::ZeroVector;
}

FRotator UWolfSnapshotComponent::GetOwnerRotation() const
{
	return OwnerCharacter ? OwnerCharacter->GetActorRotation() : FRotator::ZeroRotator;
}

UAnimInstance* UWolfSnapshotComponent::GetAnimInst() const
{
	return OwnerCharacter ? OwnerCharacter->GetAnimInst() : nullptr;
}

UAnimMontage* UWolfSnapshotComponent::GetCurrentMontage() const
{
	return GetAnimInst() ? GetAnimInst()->GetCurrentActiveMontage() : nullptr;
}

UCombatModeSubsystem* UWolfSnapshotComponent::GetCMS() const
{
	return OwnerCharacter ? OwnerCharacter->GetCMS() : nullptr;
}
