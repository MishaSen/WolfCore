// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCore/Public/Character/WolfCharacterBase.h"

#include "AIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystem/WolfAttributeSet.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/WolfAbilityComponent.h"
#include "Core/WolfFunctionLibrary.h"
#include "Core/WolfGameplayTags.h"
#include "Core/WolfPresageComponent.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Presage/ActorSnapshot.h"
#include "Systems/CombatModeSubsystem.h"
#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"
#include "WolfCore/Public/Presage/PresageAbilityRequest.h"

AWolfCharacterBase::AWolfCharacterBase() 
{
	PrimaryActorTick.bCanEverTick = true;

	AbilityControl = CreateDefaultSubobject<UWolfAbilityComponent>(TEXT("AbilityControl"));
	PresageControl = CreateDefaultSubobject<UWolfPresageComponent>(TEXT("PresageControl"));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
}

void AWolfCharacterBase::BeginPlay()
{
	Super::BeginPlay();

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

UCombatModeSubsystem* AWolfCharacterBase::GetCMS() const
{
	if (CachedCMS.IsValid()) return CachedCMS.Get();

	const auto* World = GetWorld();
	if (!IsValid(World)) return nullptr;

	auto* Subsystem = UWolfFunctionLibrary::GetWorldSubsystem<UCombatModeSubsystem>(World);
	this->CachedCMS = Subsystem;
	return Subsystem;
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
	const auto* BakedState = PresageControl->GetSnapshotAtTime(PreviewTime);
	if (!BakedState)
	{
		const auto FutureTransform = GetProjectedTransform(PreviewTime);
		WOLF_LOG(Log, TEXT("%s [FALLBACK MATH] pos at %.2f: %s"), *GetName(), PreviewTime, *FutureTransform.GetLocation().ToString());
		return;
	}
	WOLF_LOG(Log, TEXT("%s [BAKED] pos at %.2f: %s"), *GetName(), PreviewTime, *BakedState->Location.ToString());
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
	auto* ASC = GetAbilitySystemComponent();
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

void AWolfCharacterBase::CreateSnapshot_Implementation(FActorSnapshot& NewSnapshot)
{
	if (PresageControl) PresageControl->CreateSnapshot_Implementation(NewSnapshot);
}

void AWolfCharacterBase::RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot)
{
	if (PresageControl) PresageControl->RestoreSnapshot_Implementation(Snapshot);
}