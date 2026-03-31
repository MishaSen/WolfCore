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

void UWolfPresageComponent::SimulateTick(float DeltaTime)
{
	SimulatePhysicsStep(DeltaTime);
	SimulateAnimationStep(DeltaTime);

	FActorSnapshot FutureFrame;
	CreateSnapshot_Implementation(FutureFrame);
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

void UWolfPresageComponent::CreateSnapshot_Implementation(FActorSnapshot& OutSnapshot)
{
	OutSnapshot.ActorRef = CharacterOwner.Get();
	SnapshotPhysics(OutSnapshot);
	SnapshotGAS(OutSnapshot);
	SnapshotAnim(OutSnapshot);
}

void UWolfPresageComponent::RestoreSnapshot_Implementation(const FActorSnapshot& InSnapshot)
{
	bIsRestoringSnapshot = true;

	RestorePhysics(InSnapshot);
	RestoreGAS(InSnapshot);
	RestoreAnim(InSnapshot);

	bIsRestoringSnapshot = false;
	// In UI or animation code, check if (bIsRestoringSnapshot) return; before doing effects
}

void UWolfPresageComponent::SimulatePhysicsStep(float DeltaTime)
{
	if (!IsValid(GetCMS()) || GetMoveComp()->Velocity.IsNearlyZero()) return;

	const FVector Start = SimulationTransform.GetLocation();
	const FVector Delta = GetMoveComp()->Velocity * DeltaTime;
	const FVector End = Start + Delta;

	FHitResult Hit(1.f);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);

	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End,
		SimulationTransform.GetRotation(),
		ECC_Pawn,
		CharacterOwner->GetCapsuleComponent()->GetCollisionShape(),
		Params);

	if (bHit)
	{
		const auto RemainingDelta = Delta * (1.f - Hit.Time);
		const auto SlideDelta = FVector::VectorPlaneProject(RemainingDelta, Hit.Normal);

		if (!SlideDelta.IsNearlyZero())
		{
			FHitResult SlideHit;
			const auto SlideEnd = Hit.Location + SlideDelta;

			GetWorld()->SweepSingleByChannel(SlideHit, Hit.Location, SlideEnd,
				SimulationTransform.GetRotation(),
				ECC_Pawn,
				CharacterOwner->GetCapsuleComponent()->GetCollisionShape(),
				Params);
			
			SimulationTransform.SetLocation(SlideHit.bBlockingHit ? SlideHit.Location : SlideEnd);
		}
		else SimulationTransform.SetLocation(Hit.Location);
	}
	else SimulationTransform.SetLocation(End);
}

void UWolfPresageComponent::SnapshotPhysics(FActorSnapshot& Snapshot) const
{
	Snapshot.Location = GetSimLocation();
	Snapshot.Rotation = GetSimRotation();
	Snapshot.Velocity = CharacterOwner->GetVelocity();
	Snapshot.MovementMode = GetMoveComp()->MovementMode;
	Snapshot.CustomMovementMode = GetMoveComp()->CustomMovementMode;
	
	const auto* AICont = Cast<AAIController>(CharacterOwner->GetController());
	if (!IsValid(AICont)) return;
	
	const auto* PathFollowComp = AICont->GetPathFollowingComponent();
	if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving)
	{
		Snapshot.Destination = PathFollowComp->GetPathDestination();
		Snapshot.bIsMoving = true;
		
		DrawDebugSphere(GetWorld(), Snapshot.Destination, 25.f, 12, FColor::Red, false, 5.f);
		DrawDebugLine(GetWorld(), GetOwnerLocation(), Snapshot.Destination, FColor::Red,
			false, 5.f, 0, 2.f);
	}

	if (const auto* BB = AICont->GetBlackboardComponent())
	{
		Snapshot.TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
		if (Snapshot.TargetActor.IsValid())
		{
			const auto TargetBox = Snapshot.TargetActor->GetRootComponent()->Bounds.GetBox();
			DrawDebugBox(GetWorld(), TargetBox.GetCenter(), TargetBox.GetExtent(), FColor::Orange, false,
				5.f, 0, 3.f);
		}
	}

	WOLF_LOG(Log, TEXT("Character %s has Path Destination %s targeting %s. IsMoving = %s."),
		*GetName(),
		*Snapshot.Destination.ToString(),
		*Snapshot.TargetActor->GetName(),
		Snapshot.bIsMoving ? TEXT ("True") : TEXT("False"));
}

void UWolfPresageComponent::RestorePhysics(const FActorSnapshot& Snapshot)
{
	CharacterOwner->SetActorLocationAndRotation
	(
		Snapshot.Location,
		Snapshot.Rotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);
	
	auto* Capsule = CharacterOwner->GetCapsuleComponent();
	if (IsValid(Capsule))
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	}

	if (IsValid(GetMoveComp()))
	{
		GetMoveComp()->SetComponentTickEnabled(true);
		GetMoveComp()->Activate();

		GetMoveComp()->SetMovementMode(Snapshot.MovementMode, Snapshot.CustomMovementMode);
		GetMoveComp()->Velocity = Snapshot.Velocity;
		GetMoveComp()->UpdateComponentVelocity();
	}
	
	auto* PrimitiveComp = Cast<UPrimitiveComponent>(CharacterOwner->GetRootComponent());
	if (IsValid(PrimitiveComp))
	{
		if (!PrimitiveComp->IsSimulatingPhysics()) return;
		PrimitiveComp->SetPhysicsLinearVelocity(Snapshot.Velocity);
	}
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

void UWolfPresageComponent::SnapshotAnim(FActorSnapshot& Snapshot) const
{
	if (auto* CurrentMontage = GetCurrentMontage())
	{
		Snapshot.CurrentMontage = CurrentMontage;
		Snapshot.MontagePosition = GetAnimInst()->Montage_GetPosition(CurrentMontage);
	}
}

void UWolfPresageComponent::RestoreAnim(const FActorSnapshot& Snapshot)
{
	if (auto* AnimInst = GetAnimInst())
	{
		AnimInst->StopAllMontages(0.f);
		if (!Snapshot.CurrentMontage.IsValid()) return;

		AnimInst->Montage_Play(Snapshot.CurrentMontage.Get(), 1.f);
		AnimInst->Montage_SetPosition(Snapshot.CurrentMontage.Get(), Snapshot.MontagePosition);
	}
}

void UWolfPresageComponent::SnapshotGAS(FActorSnapshot& Snapshot) const
{
	if (!IsValid(CachedASC)) return;

	const auto& Attributes = AbilityControl->GetCachedAttributes();
	
	Snapshot.AttributeValues.Empty(Attributes.Num());
	for (const auto& Attribute : Attributes)
	{
		Snapshot.AttributeValues.Add(CachedASC->GetNumericAttribute(Attribute));
	}

	Snapshot.ActiveEffects.Reset();
	const FGameplayEffectQuery Query;
	const auto ActiveHandles = CachedASC->GetActiveEffects(Query);
	Snapshot.ActiveEffects.Reserve(ActiveHandles.Num());

	for (const auto& Handle : ActiveHandles)
	{
		if (const auto* Effect = CachedASC->GetActiveGameplayEffect(Handle))
		{
			FStoredEffect StoredEffect;
			StoredEffect.EffectClass = Effect->Spec.Def.GetClass();
			StoredEffect.Level = Effect->Spec.GetLevel();
			StoredEffect.Stacks = Effect->Spec.GetStackCount();
			StoredEffect.RemainingDuration = Effect->GetDuration() > 0.f
				                                 ? Effect->GetTimeRemaining(GetWorld()->GetTimeSeconds())
				                                 : -1.f;

			Snapshot.ActiveEffects.Add(StoredEffect);
		}
	}
}

void UWolfPresageComponent::RestoreGAS(const FActorSnapshot& Snapshot)
{
	if (!IsValid(CachedASC) || !AbilityControl->GetStatConfig()) return;

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

	FGameplayEffectQuery Query;
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(FWolfGameplayTags::Get().Effect_Combat);
	Query.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(TagContainer);
	CachedASC->RemoveActiveEffects(Query); // Don't want to remove effects like Presage

	for (const auto& Effect : Snapshot.ActiveEffects)
	{
		if (!Effect.EffectClass) continue;

		auto SpecHandle = CachedASC->MakeOutgoingSpec(Effect.EffectClass, Effect.Level, CachedASC->MakeEffectContext());
		if (!SpecHandle.IsValid()) continue;

		SpecHandle.Data->SetStackCount(Effect.Stacks);

		if (Effect.RemainingDuration > 0.f)
		{
			SpecHandle.Data->Duration = Effect.RemainingDuration;
		}
		CachedASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	// TODO: When we implement the UI Controller, remember to check this bool when doing delegate broadcasts
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