// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/WolfPresageComponent.h"

#include "AIController.h"
#include "Character/WolfCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Core/WolfAbilityComponent.h"
#include "Core/WolfFunctionLibrary.h"
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
		AbilityControl = CharacterOwner->FindComponentByClass<UWolfAbilityComponent>();
		CachedMoveComp = CharacterOwner->GetCharacterMovement();
		if (const auto* Mesh = CharacterOwner->GetMesh()) CachedAnimInst = Mesh->GetAnimInstance();
	}
}

UWolfAbilitySystemComponent* UWolfPresageComponent::GetWolfASC() const
{
	if (IsValid(CachedASC)) return CachedASC;

	if (IsValid(AbilityControl)) const_cast<UWolfPresageComponent*>(this)->CachedASC = AbilityControl->GetWolfASC();
	return CachedASC;
}

void UWolfPresageComponent::SimulateTick(float DeltaTime)
{
	SimulatePhysicsStep(DeltaTime);
	SimulateAnimationStep(DeltaTime);

	FActorSnapshot FutureFrame;
	CreateSnapshot_Implementation(FutureFrame);
	PredictionBuffer.Add(FutureFrame); // Remember to clear PredictionBuffer in CombatModeSubsystem
}

void UWolfPresageComponent::SimulatePhysicsStep(float DeltaTime)
{
	if (!CachedCMS.IsValid() || CachedMoveComp->Velocity.IsNearlyZero()) return;

	const auto Delta = CachedMoveComp->Velocity * DeltaTime;
	FHitResult Hit(1.f);
	CachedMoveComp->SafeMoveUpdatedComponent(Delta, GetOwnerRotation(), true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		const auto RemainingDelta = Delta * (1.f - Hit.Time);
		const auto SlideDelta = FVector::VectorPlaneProject(RemainingDelta, Hit.Normal);

		if (!SlideDelta.IsNearlyZero())
		{
			FHitResult SlideHit(1.f);
			CachedMoveComp->SafeMoveUpdatedComponent(SlideDelta, GetOwnerRotation(), true, SlideHit);
		}
	}
}

void UWolfPresageComponent::SimulateAnimationStep(float DeltaTime)
{
	const auto* CurrentMontage = GetWolfCurrentMontage();
	if (!IsValid(CurrentMontage)) return;

	const auto CurrentPos = CachedAnimInst->Montage_GetPosition(CurrentMontage);
	const auto NewPos = CurrentPos + DeltaTime;

	CachedAnimInst->Montage_SetPosition(CurrentMontage, NewPos);

	if (CurrentMontage->HasRootMotion())
	{
		const auto RootMotionDelta = CurrentMontage->ExtractRootMotionFromRange(CurrentPos,NewPos, FAnimExtractContext());
		const auto WorldDelta = GetOwnerRotation().RotateVector(RootMotionDelta.GetLocation());

		FHitResult Hit;
		CharacterOwner->GetCharacterMovement()->SafeMoveUpdatedComponent(WorldDelta, GetOwnerRotation(), true, Hit);
	}

	if (NewPos >= CurrentMontage->GetPlayLength())
	{
		CachedAnimInst->Montage_Stop(0.1f, CurrentMontage); // If seeing t-poses, set to idle state
	}
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

void UWolfPresageComponent::SnapshotPhysics(FActorSnapshot& Snapshot) const
{
	Snapshot.Location = GetOwnerLocation();
	Snapshot.Rotation = GetOwnerRotation();
	Snapshot.Velocity = CharacterOwner->GetVelocity();
	Snapshot.MovementMode = CachedMoveComp->MovementMode;
	Snapshot.CustomMovementMode = CachedMoveComp->CustomMovementMode;

	if (const auto* AICont = Cast<AAIController>(CharacterOwner->GetController()))
	{
		const auto* PathFollowComp = AICont->GetPathFollowingComponent();
		if (!PathFollowComp || PathFollowComp->GetStatus() != EPathFollowingStatus::Moving) return;
		
		Snapshot.AIMoveTarget = PathFollowComp->GetPathDestination();
		Snapshot.bIsMoving = true;
		
		DrawDebugSphere(GetWorld(), Snapshot.AIMoveTarget, 25.f, 12, FColor::Red, false, 5.f);
		DrawDebugLine(GetWorld(), GetOwnerLocation(), Snapshot.AIMoveTarget, FColor::Red,
			false, 5.f, 0, 2.f);
		WOLF_LOG(Log, TEXT("Character %s has Path Destination %s. IsMoving = %s."),
				*GetName(),
				*Snapshot.AIMoveTarget.ToString(),
				Snapshot.bIsMoving ? TEXT ("True") : TEXT("False"));
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

void UWolfPresageComponent::SnapshotAnim(FActorSnapshot& Snapshot) const
{
	if (auto* CurrentMontage = GetWolfCurrentMontage())
	{
		Snapshot.CurrentMontage = CurrentMontage;
		Snapshot.MontagePosition = CachedAnimInst->Montage_GetPosition(CurrentMontage);
	}
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

	if (IsValid(CachedMoveComp))
	{
		CachedMoveComp->SetComponentTickEnabled(true);
		CachedMoveComp->Activate();

		CachedMoveComp->SetMovementMode(Snapshot.MovementMode, Snapshot.CustomMovementMode);
		CachedMoveComp->Velocity = Snapshot.Velocity;
		CachedMoveComp->UpdateComponentVelocity();
	}
	
	auto* PrimitiveComp = Cast<UPrimitiveComponent>(CharacterOwner->GetRootComponent());
	if (IsValid(PrimitiveComp))
	{
		if (!PrimitiveComp->IsSimulatingPhysics()) return;
		PrimitiveComp->SetPhysicsLinearVelocity(Snapshot.Velocity);
	}
}

void UWolfPresageComponent::RestoreGAS(const FActorSnapshot& Snapshot)
{
	if (!IsValid(GetWolfASC()) || !AbilityControl->GetStatConfig()) return;

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

void UWolfPresageComponent::RestoreAnim(const FActorSnapshot& Snapshot)
{
	if (CachedAnimInst)
	{
		CachedAnimInst->StopAllMontages(0.f);
		if (!Snapshot.CurrentMontage.IsValid()) return;

		CachedAnimInst->Montage_Play(Snapshot.CurrentMontage.Get(), 1.f);
		CachedAnimInst->Montage_SetPosition(Snapshot.CurrentMontage.Get(), Snapshot.MontagePosition);
	}
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

UCombatModeSubsystem* UWolfPresageComponent::GetCMS() const
{
	if (CachedCMS.IsValid()) return CachedCMS.Get();

	const auto* World = GetWorld();
	if (!World) return nullptr;

	auto* Subsystem = UWolfFunctionLibrary::GetWorldSubsystem<UCombatModeSubsystem>(World);
	this->CachedCMS = Subsystem;
	return Subsystem;
}