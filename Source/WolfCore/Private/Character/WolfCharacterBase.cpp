// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCore/Public/Character/WolfCharacterBase.h"

#include "AIController.h"
#include "Abilities/BaseCombatAbility.h"
#include "AbilitySystem/WolfAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/WolfAbilityComponent.h"
#include "Core/WolfFunctionLibrary.h"
#include "Core/WolfGameplayTags.h"
#include "Core/WolfPresageComponent.h"
#include "Core/WolfSnapshotComponent.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Presage/ActorSnapshot.h"
#include "Systems/CombatModeSubsystem.h"
#include "Interfaces/IWolfCombatant.h"

AWolfCharacterBase::AWolfCharacterBase() 
{
	PrimaryActorTick.bCanEverTick = true;

	AbilityControl = CreateDefaultSubobject<UWolfAbilityComponent>(TEXT("AbilityControl"));
	PresageControl = CreateDefaultSubobject<UWolfPresageComponent>(TEXT("PresageControl"));
	SnapshotControl = CreateDefaultSubobject<UWolfSnapshotComponent>(TEXT("SnapshotControl"));

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
	CachedCMS = UWolfFunctionLibrary::GetWorldSubsystem<UCombatModeSubsystem>(GetWorld());

	if (auto* CMS = GetCMS())
	{
		// Register using TScriptInterface<IWolfCombatant> for interface-based decoupling.
		// This is the only thing needed to sync the character's initial state via the subsystem.
		CMS->RegisterCombatant(TScriptInterface<IWolfCombatant>(this));
	}
}

void AWolfCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (auto* CMS = GetCMS())
	{
		// Unregister only - the interface-based OnCombatModeChanged is handled by the subsystem directly.
		CMS->UnregisterCombatant(TScriptInterface<IWolfCombatant>(this));
	}
	Super::EndPlay(EndPlayReason);
}

UCombatModeSubsystem* AWolfCharacterBase::GetCMS() const
{
	return CachedCMS.Get();
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
		
		GrantedAbilityHandles = AbilityControl->AddStartupAbilities(StartupAbilities);

		// Populate GrantedAbilityTags for secondary lookups
		GrantedAbilityTags.Empty();
		for (const auto& [AbilityClass, Handle] : GrantedAbilityHandles)
		{
			if (const auto* AbilityCDO = Cast<UBaseCombatAbility>(AbilityClass->GetDefaultObject()))
			{
				GrantedAbilityTags.Add(AbilityCDO->StartupInputTag);
			}
		}

		WOLF_LOG(Log, TEXT("Character %s has been possessed by %s."), *GetName(), *NewController->GetName());
	}
}

// IWolfCombatant Interface Implementations

bool AWolfCharacterBase::IsKillable() const
{
	auto* ASC = GetAbilitySystemComponent();
	if (!ASC) return false;

	// Stateless query: check if dead or invulnerable tags exist on the ASC.
	return !ASC->HasMatchingGameplayTag(FWolfGameplayTags::Get().InputState_Dead)
		   && !ASC->HasMatchingGameplayTag(FWolfGameplayTags::Get().InputState_Invulnerable);
}

FGameplayTag AWolfCharacterBase::GetCurrentCombatMode() const
{
	auto* ASC = GetAbilitySystemComponent();
	if (!ASC) return FWolfGameplayTags::Get().InputState_RT;

	// Use HasMatchingGameplayTag directly for performance.
	if (ASC->HasMatchingGameplayTag(FWolfGameplayTags::Get().InputState_TB))
	{
		return FWolfGameplayTags::Get().InputState_TB;
	}
	return FWolfGameplayTags::Get().InputState_RT;
}

UAbilitySystemComponent* AWolfCharacterBase::GetASC() const
{
	return GetAbilitySystemComponent();
}

void AWolfCharacterBase::OnTriggerDeath()
{
	Die();
}

/**
 * @brief Handles notification when the global combat mode changes.
 * This is a local-only reaction (e.g., UI updates, VFX). Does NOT call the Subsystem.
 */
void AWolfCharacterBase::OnCombatModeChanged_Implementation(FGameplayTag NewMode)
{
	// Local-only reactions: UI updates, VFX, or other client-side effects.
	// The subsystem is the source of truth; this character simply reacts to state changes.
}

UWolfPresageComponent* AWolfCharacterBase::GetPresageComponent() const
{
	return PresageControl;
}

float AWolfCharacterBase::GetActiveAbilityProgress() const
{
	if (const auto* CurrentAbility = GetActiveCombatAbility())
	{
		return CurrentAbility->GetPeriodProgress();
	}
	return 0.f;
}

void AWolfCharacterBase::Die_Implementation()
{
	if (!IsValid(AbilityControl)) return;
	
	auto* WolfASC = AbilityControl->GetWolfASC();
	if (!IsValid(WolfASC)) return;
	if (WolfASC->HasMatchingGameplayTag(FWolfGameplayTags::Get().InputState_Dead)) return; // Already dead.

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

	WolfASC->CancelAllAbilities();
	WolfASC->AddLooseGameplayTag(FWolfGameplayTags::Get().InputState_Dead);

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

	const auto& ActorStates = CMS->GetMasterSnapshot().ActorStates;
	const auto* AnchorStatePtr = ActorStates.Find(this);
	if (!AnchorStatePtr) return GetActorTransform();

	const auto& AnchorState = *AnchorStatePtr;
	
	if (!AnchorState.CurrentMontage.IsValid())
	{
		const auto ProjectedLocation = AnchorState.Location + AnchorState.Velocity * FutureTimeDelta;
		return FTransform(AnchorState.Rotation, ProjectedLocation, GetActorScale3D());
	}

	const FTransform AnchorTransform(AnchorState.Rotation, AnchorState.Location, GetActorScale3D());

	return UWolfFunctionLibrary::GetProjectedTransform(
		CachedAnimInst,
		AnchorState.CurrentMontage.Get(),
		AnchorState.MontagePosition + FutureTimeDelta,
		AnchorTransform);
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
	if (!CurrentMontage || !CachedAnimInst) return -1.f;

	const auto CurrentPosition = CachedAnimInst->Montage_GetPosition(CurrentMontage);

	for (const auto& NotifyEvent : CurrentMontage->Notifies)
	{
		if (NotifyEvent.GetTriggerTime() > CurrentPosition)
		{
			return NotifyEvent.GetTriggerTime() - CurrentPosition; // Could filter for specific hit notifies.
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
	if (SnapshotControl) ISnapshot::Execute_CreateSnapshot(SnapshotControl, NewSnapshot);
}

void AWolfCharacterBase::RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot)
{
	if (SnapshotControl) ISnapshot::Execute_RestoreSnapshot(SnapshotControl, Snapshot);
}
