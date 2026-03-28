// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCore/Public/Character/WolfCharacterBase.h"

#include "AIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystem/WolfAttributeSet.h"
#include "AttributeSet.h"
#include "AbilitySystem/CharacterStatConfig.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/WolfAbilityComponent.h"
#include "Core/WolfFunctionLibrary.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Presage/ActorSnapshot.h"
#include "Systems/CombatModeSubsystem.h"
#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"
#include "WolfCore/Public/Presage/PresageAbilityRequest.h"

AWolfCharacterBase::AWolfCharacterBase() // TODO: Fat Class. Split.
{
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponentClass = UWolfAbilitySystemComponent::StaticClass();
	AbilityControl = CreateDefaultSubobject<UWolfAbilityComponent>(TEXT("AbilityControl"));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
}

void AWolfCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	CachedMoveComp = Cast<UCharacterMovementComponent>(GetCharacterMovement());
	CachedAnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

	if (auto* CMS = GetCMS())
	{
		CMS->RegisterCombatant(this);
		CMS->OnCombatModeChanged.AddDynamic(this, &AWolfCharacterBase::HandleCombatModeChanged);
	}
}

void AWolfCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (auto* CMS = GetCMS())
	{
		// RemoveDynamic is fine here, but the UE delegate macro should handle this automatically
		CMS->OnCombatModeChanged.RemoveDynamic(this, &AWolfCharacterBase::HandleCombatModeChanged);
		CMS->UnregisterCombatant(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AWolfCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasAuthority() && IsValid(AbilityControl))
	{
		AbilityControl->InitializeAbilitySystem(this);

		if (auto* ASC = AbilityControl->GetWolfASC())
		{
			ASC->InitAbilityActorInfo(this, this);
		}
		AbilityControl->ApplyDefaultAttributes();
		AbilityControl->AddStartupAbilities(StartupAbilities);

		WOLF_LOG(Log, TEXT("Character %s has been possessed by %s."), *GetName(), *NewController->GetName());
	}
}

void AWolfCharacterBase::HandleCombatModeChanged(FGameplayTag NewMode)
{
	auto* CMS = GetCMS();
	if (!IsValid(CMS)) return;
	CMS->ApplyModeToActor(this, NewMode);

	if (const auto* AIControl = Cast<AAIController>(GetController()))
	{
		auto* BTComp = Cast<UBehaviorTreeComponent>(AIControl->GetBrainComponent());
		auto* PathFollowComp = AIControl->GetPathFollowingComponent();
		if (!IsValid(BTComp) || !PathFollowComp) return; // PFComp should be valid; separate check if buggy.

		if (CMS->bIsInTB)
		{
			BTComp->PauseLogic(TEXT("Entering TB"));
			PathFollowComp->PauseMove();
		}
		else
		{
			BTComp->ResumeLogic(TEXT("Exiting TB"));
			PathFollowComp->ResumeMove();
		}
	}
}

void AWolfCharacterBase::Die_Implementation()
{
	auto* WolfASC = AbilityControl ? AbilityControl->GetWolfASC() : nullptr;
	if (WolfASC && WolfASC->HasMatchingGameplayTag(FWolfGameplayTags::Get().InputState_Dead)) return; // Already dead.

	if (auto* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	if (auto* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	if (IsValid(WolfASC))
	{
		WolfASC->CancelAllAbilities();
		WolfASC->AddLooseGameplayTag(FWolfGameplayTags::Get().InputState_Dead);
	}

	WOLF_INFO(TEXT("Character %s has died."), *GetName());
}

FGameplayAbilitySpecHandle AWolfCharacterBase::GetAbilitySpecHandle(const TSubclassOf<UGameplayAbility>& AbilityClass) const
{
	if (GrantedAbilityHandles.Contains(AbilityClass))
	{
		return GrantedAbilityHandles[AbilityClass];
	}
	return FGameplayAbilitySpecHandle();
}

UCombatModeSubsystem* AWolfCharacterBase::GetCMS() const
{
	if (!CachedCMS.IsValid())
	{
		const_cast<AWolfCharacterBase*>(this)->CachedCMS = UWolfFunctionLibrary::GetWorldSubsystem<UCombatModeSubsystem>(this);
	}
	return CachedCMS.Get();
}

FTransform AWolfCharacterBase::GetProjectedTransform(float FutureTimeDelta) const
{
	const auto* CMS = GetCMS();
	if (!CMS) return GetActorTransform();

	const auto* ActorStates = &CMS->GetMasterSnapshot().ActorStates;
	if (!ActorStates->Contains(this)) return GetActorTransform();

	const auto& AnchorState = (*ActorStates)[this];
	if (!AnchorState.CurrentMontage.IsValid())
	{
		const auto ProjectedLocation = AnchorState.Location + AnchorState.Velocity * FutureTimeDelta;
		return FTransform(AnchorState.Rotation, ProjectedLocation, GetActorScale3D());
	}

	const auto* Montage = AnchorState.CurrentMontage.Get();
	const float TargetPosition = AnchorState.MontagePosition + FutureTimeDelta;

	const auto RootMotionDelta = Montage->ExtractRootMotionFromRange(
		AnchorState.MontagePosition,
		TargetPosition,
		FAnimExtractContext()
	);

	const FTransform AnchorTransform(AnchorState.Rotation, AnchorState.Location, GetActorScale3D());
	return RootMotionDelta * AnchorTransform;
}

void AWolfCharacterBase::UpdateTemporalPreview(float PreviewTime)
{
	const auto* BakedState = GetSnapshotAtTime(PreviewTime);
	if (!BakedState)
	{
		const auto FutureTransform = GetProjectedTransform(PreviewTime);
		WOLF_LOG(Log, TEXT("%s [FALLBACK MATH] pos at %.2f: %s"), *GetName(), PreviewTime, *FutureTransform.GetLocation().ToString());
		return;
	}
	WOLF_LOG(Log, TEXT("%s [BAKED] pos at %.2f: %s"), *GetName(), PreviewTime, *BakedState->Location.ToString());
}

const FActorSnapshot* AWolfCharacterBase::GetSnapshotAtTime(float RelativeTime) const
{
	if (PredictionBuffer.Num() == 0) return nullptr;

	const int32 Index = FMath::Clamp(FMath::RoundToInt(RelativeTime * WolfSimConfig::Frequency),
								// Snapshot lookup will drift if the Subsystem uses a variable step or different fixed rate.
							   0,
							   PredictionBuffer.Num() - 1);
	return &PredictionBuffer[Index];
}

void AWolfCharacterBase::SimulateTick(float DeltaTime)
{
	SimulatePhysicsStep(DeltaTime);
	SimulateAnimationStep(DeltaTime);

	FActorSnapshot FutureFrame;
	CreateSnapshot_Implementation(FutureFrame);
	PredictionBuffer.Add(FutureFrame); // Remember to clear PredictionBuffer in CombatModeSubsystem
}

void AWolfCharacterBase::SimulatePhysicsStep(float DeltaTime)
{
	if (!CachedCMS.IsValid() || CachedMoveComp->Velocity.IsNearlyZero()) return;

	const auto Delta = CachedMoveComp->Velocity * DeltaTime;
	FHitResult Hit(1.f);
	CachedMoveComp->SafeMoveUpdatedComponent(Delta, GetActorRotation(), true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		const auto RemainingDelta = Delta * (1.f - Hit.Time);
		const auto SlideDelta = FVector::VectorPlaneProject(RemainingDelta, Hit.Normal);

		if (!SlideDelta.IsNearlyZero())
		{
			FHitResult SlideHit(1.f);
			CachedMoveComp->SafeMoveUpdatedComponent(SlideDelta, GetActorRotation(), true, SlideHit);
		}
	}
}

void AWolfCharacterBase::SimulateAnimationStep(float DeltaTime)
{
	const auto* CurrentMontage = GetWolfCurrentMontage();
	if (!IsValid(CurrentMontage)) return;

	const auto CurrentPos = CachedAnimInst->Montage_GetPosition(CurrentMontage);
	const auto NewPos = CurrentPos + DeltaTime;

	CachedAnimInst->Montage_SetPosition(CurrentMontage, NewPos);

	if (CurrentMontage->HasRootMotion())
	{
		const auto RootMotionDelta = CurrentMontage->ExtractRootMotionFromRange(CurrentPos, NewPos, FAnimExtractContext());
		const auto WorldDelta = GetActorRotation().RotateVector(RootMotionDelta.GetLocation());

		FHitResult Hit;
		GetCharacterMovement()->SafeMoveUpdatedComponent(WorldDelta, GetActorRotation(), true, Hit);
	}

	if (NewPos >= CurrentMontage->GetPlayLength())
	{
		CachedAnimInst->Montage_Stop(0.1f, CurrentMontage); // If seeing t-poses, set to idle state
	}
}

void AWolfCharacterBase::CreateSnapshot_Implementation(FActorSnapshot& OutSnapshot)
{
	OutSnapshot.ActorRef = this;
	SnapshotPhysics(OutSnapshot);
	SnapshotGAS(OutSnapshot);
	SnapshotAnim(OutSnapshot);
}

void AWolfCharacterBase::RestoreSnapshot_Implementation(const FActorSnapshot& InSnapshot)
{
	bIsRestoringSnapshot = true;

	RestorePhysics(InSnapshot);
	RestoreGAS(InSnapshot);
	RestoreAnim(InSnapshot);

	bIsRestoringSnapshot = false;
	// In UI or animation code, check if (bIsRestoringSnapshot) return; before doing effects
}

void AWolfCharacterBase::SnapshotPhysics(FActorSnapshot& Snapshot) const
{
	Snapshot.Location = GetActorLocation();
	Snapshot.Rotation = GetActorRotation();
	Snapshot.Velocity = GetVelocity();
	Snapshot.MovementMode = CachedMoveComp->MovementMode;
	Snapshot.CustomMovementMode = CachedMoveComp->CustomMovementMode;

	if (const auto* AICont = Cast<AAIController>(GetController()))
	{
		const auto* PathFollowComp = AICont->GetPathFollowingComponent();
		if (!PathFollowComp || PathFollowComp->GetStatus() != EPathFollowingStatus::Moving) return;
		
		Snapshot.AIMoveTarget = PathFollowComp->GetPathDestination();
		Snapshot.bIsMoving = true;
		
		DrawDebugSphere(GetWorld(), Snapshot.AIMoveTarget, 25.f, 12, FColor::Red, false, 5.f);
		DrawDebugLine(GetWorld(), GetActorLocation(), Snapshot.AIMoveTarget, FColor::Red,
			false, 5.f, 0, 2.f);
		WOLF_LOG(Log, TEXT("Character %s has Path Destination %s. IsMoving = %s."),
				*GetName(),
				*Snapshot.AIMoveTarget.ToString(),
				Snapshot.bIsMoving ? TEXT ("True") : TEXT("False"));
	}
}

void AWolfCharacterBase::SnapshotGAS(FActorSnapshot& Snapshot) const
{
	if (!IsValid(ASC)) return;

	Snapshot.AttributeValues.Empty(CachedAttributes.Num());
	for (const auto& Attribute : CachedAttributes)
	{
		Snapshot.AttributeValues.Add(ASC->GetNumericAttribute(Attribute));
	}

	Snapshot.ActiveEffects.Reset();
	const FGameplayEffectQuery Query;
	const auto ActiveHandles = ASC->GetActiveEffects(Query);
	Snapshot.ActiveEffects.Reserve(ActiveHandles.Num());

	for (const auto& Handle : ActiveHandles)
	{
		if (const auto* Effect = ASC->GetActiveGameplayEffect(Handle))
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

void AWolfCharacterBase::SnapshotAnim(FActorSnapshot& Snapshot) const
{
	if (auto* CurrentMontage = GetWolfCurrentMontage())
	{
		Snapshot.CurrentMontage = CurrentMontage;
		Snapshot.MontagePosition = CachedAnimInst->Montage_GetPosition(CurrentMontage);
	}
}

void AWolfCharacterBase::RestorePhysics(const FActorSnapshot& Snapshot)
{
	SetActorLocationAndRotation
	(
		Snapshot.Location,
		Snapshot.Rotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	if (auto* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	}

	if (CachedMoveComp)
	{
		CachedMoveComp->SetComponentTickEnabled(true);
		CachedMoveComp->Activate();

		CachedMoveComp->SetMovementMode(Snapshot.MovementMode, Snapshot.CustomMovementMode);
		CachedMoveComp->Velocity = Snapshot.Velocity;
		CachedMoveComp->UpdateComponentVelocity();
	}

	if (auto* PrimitiveComp = Cast<UPrimitiveComponent>(GetRootComponent()))
	{
		if (!PrimitiveComp->IsSimulatingPhysics()) return;
		PrimitiveComp->SetPhysicsLinearVelocity(Snapshot.Velocity);
	}
}

void AWolfCharacterBase::RestoreGAS(const FActorSnapshot& Snapshot)
{
	if (!IsValid(ASC) || !IsValid(StatConfig)) return;
	ASC->SetTagMapCount(FWolfGameplayTags::Get().InputState_Dead, 0);

	for (int32 StatIndex = 0; StatIndex < CachedAttributes.Num(); ++StatIndex)
	{
		if (!Snapshot.AttributeValues.IsValidIndex(StatIndex)) break;

		const auto& Attribute = CachedAttributes[StatIndex];
		const auto SavedValue = Snapshot.AttributeValues[StatIndex];

		if (!FMath::IsNearlyEqual(ASC->GetNumericAttribute(Attribute), SavedValue))
		{
			ASC->SetNumericAttributeBase(Attribute, SavedValue);
		}
	}

	FGameplayEffectQuery Query;
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(FWolfGameplayTags::Get().Effect_Combat);
	Query.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(TagContainer);
	ASC->RemoveActiveEffects(Query); // Don't want to remove effects like Presage

	for (const auto& Effect : Snapshot.ActiveEffects)
	{
		if (!Effect.EffectClass) continue;

		auto SpecHandle = ASC->MakeOutgoingSpec(Effect.EffectClass, Effect.Level, ASC->MakeEffectContext());
		if (!SpecHandle.IsValid()) continue;

		SpecHandle.Data->SetStackCount(Effect.Stacks);

		if (Effect.RemainingDuration > 0.f)
		{
			SpecHandle.Data->Duration = Effect.RemainingDuration;
		}
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	// TODO: When we implement the UI Controller, remember to check this bool when doing delegate broadcasts
}

void AWolfCharacterBase::RestoreAnim(const FActorSnapshot& Snapshot)
{
	if (CachedAnimInst)
	{
		CachedAnimInst->StopAllMontages(0.f);
		if (!Snapshot.CurrentMontage.IsValid()) return;

		CachedAnimInst->Montage_Play(Snapshot.CurrentMontage.Get(), 1.f);
		CachedAnimInst->Montage_SetPosition(Snapshot.CurrentMontage.Get(), Snapshot.MontagePosition);
	}
}

float AWolfCharacterBase::GetTimeToNextHitImpact() const
{
	auto* CurrentMontage = GetWolfCurrentMontage();
	if (!CurrentMontage) return -1.f;

	const auto CurrentMontagePosition = CachedAnimInst->Montage_GetPosition(CurrentMontage);

	for (const auto& NotifyEvent : CurrentMontage->Notifies)
	{
		if (NotifyEvent.GetTriggerTime() > CurrentMontagePosition)
		{
			return NotifyEvent.GetTriggerTime() - CurrentMontagePosition;
		}
	}
	return -1.f;
}

void AWolfCharacterBase::GetPresageCollisionDimensions(float& OutRadius, float& OutHalfHeight) const
{
	const auto* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		OutRadius = 0.f;
		OutHalfHeight = 0.f;
		return;
	}

	OutRadius = Capsule->GetScaledCapsuleRadius();
	OutHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
}

FTransform AWolfCharacterBase::ExtractRootMotionAtTime(UAnimMontage* Montage, float Time)
{
	if (!Montage) return FTransform::Identity;

	return Montage->ExtractRootMotionFromRange(0.f, Time, FAnimExtractContext());
}

bool AWolfCharacterBase::IsInvulnerableAt(float RelativeTime) const
{
	if (const auto* CurrentAbility = GetActiveCombatAbility())
	{
		return CurrentAbility->IsInvulnerableAt(RelativeTime);
	}

	return false;
}

UBaseCombatAbility* AWolfCharacterBase::GetActiveCombatAbility() const
{
	if (!ASC) return nullptr;

	for (const auto& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive()) continue;

		for (auto* Instance : Spec.GetAbilityInstances())
		{
			if (auto* CombatAbility = Cast<UBaseCombatAbility>(Instance)) return CombatAbility;
		}
	}
	return nullptr;
}