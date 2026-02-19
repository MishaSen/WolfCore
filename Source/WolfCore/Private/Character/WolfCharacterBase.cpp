// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCore/Public/Character/WolfCharacterBase.h"

#include "AIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Abilities/AbilityConfig.h"
#include "AbilitySystem/WolfAttributeSet.h"
#include "AttributeSet.h"
#include "Abilities/RTCombatAbility.h"
#include "AbilitySystem/CharacterStatConfig.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionComponent.h"
#include "Presage/ActorSnapshot.h"
#include "Systems/CombatModeSubsystem.h"
#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"
#include "WolfCore/Public/Presage/PresageAbilityRequest.h"
#include "WolfCore/Public/Presage/PresageSubsystem.h"

AWolfCharacterBase::AWolfCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponentClass = UWolfAbilitySystemComponent::StaticClass();
	AttributeSet = CreateDefaultSubobject<UWolfAttributeSet>("AttributeSet");

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
}

UAbilitySystemComponent* AWolfCharacterBase::GetAbilitySystemComponent() const
{
	return ASC.Get();
}

FGameplayAbilitySpecHandle AWolfCharacterBase::GetAbilitySpecHandle(
	const TSubclassOf<UGameplayAbility>& AbilityClass) const
{
	if (GrantedAbilityHandles.Contains(AbilityClass))
	{
		return GrantedAbilityHandles[AbilityClass];
	}
	return FGameplayAbilitySpecHandle();
}

void AWolfCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (auto* World = GetWorld())
	{
		if (auto* CMS = World->GetSubsystem<UCombatModeSubsystem>())
		{
			CMS->RegisterCombatListener(this);
		}
	}
	
	if (HasAuthority() && !IsValid(ASC))
	{
		SetupAbilitySystem();
	}
}

void AWolfCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (auto* World = GetWorld())
	{
		if (auto* CMS = World->GetSubsystem<UCombatModeSubsystem>())
		{
			CMS->UnregisterCombatListener(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AWolfCharacterBase::Die_Implementation()
{
	if (ASC && ASC->HasMatchingGameplayTag(FWolfGameplayTags::Get().InputState_Dead)) return; // Already dead.

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

	if (IsValid(ASC))
	{
		ASC->CancelAllAbilities();
		ASC->AddLooseGameplayTag(FWolfGameplayTags::Get().InputState_Dead);
	}

	WOLF_INFO(TEXT("Character %s has died."), *GetName());
}

void AWolfCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AWolfCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AWolfCharacterBase::SetupAbilitySystem()
{
	if (!AbilitySystemComponentClass)
	{
		WOLF_ERROR(TEXT("No AbilitySystemComponent set for %s"), *GetName());
		return;
	}

	ASC = NewObject<UWolfAbilitySystemComponent>(this, AbilitySystemComponentClass, TEXT("ASC"));
	if (ASC)
	{
		ASC->SetIsReplicated(false);
		ASC->RegisterComponent();
		WOLF_LOG(Log, TEXT("Created AbilitySystemComponent for %s"), *GetName());
	}
	else
	{
		WOLF_ERROR(TEXT("Failed to create AbilitySystemComponent for %s"), *GetName());
	}
}

void AWolfCharacterBase::ApplyDefaultAttributes()
{
	if (!IsValid(ASC) || !DefaultAttributes || !StatConfig) return;

	auto EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const auto DefaultStatsHandle = ASC->MakeOutgoingSpec(DefaultAttributes, 1, EffectContext);
	if (!DefaultStatsHandle.IsValid()) return;

	for (const auto& [Tag, Value] : StatConfig->DefaultStats)
	{
		DefaultStatsHandle.Data->SetSetByCallerMagnitude(Tag, Value);

		if (!UWolfAttributeSet::GetAttributeByTag(Tag).IsValid())
		{
			WOLF_WARN(TEXT("Attribute Tag [%s] is in StatConfig but NOT registered in UWolfAttributeSet mapping!"
				          "Snapshots/Presage will ignore this stat."), *Tag.ToString());
		}
	}

	ASC->ApplyGameplayEffectSpecToSelf(*DefaultStatsHandle.Data.Get());
	WOLF_LOG(Log, TEXT("Applied all attributes from StatConfig to %s"), *GetName());

	// --- Passive Adrenaline ---
	if (!PassiveAdrenalineGE)
	{
		WOLF_WARN(TEXT("PassiveAdrenalineGE is invalid for %s"), *GetName());
	}

	if (const auto PassiveAdrenalineHandle = ASC->MakeOutgoingSpec(PassiveAdrenalineGE, 1.f, ASC->MakeEffectContext());
		PassiveAdrenalineHandle.IsValid())
	{
		PassiveAdrenalineHandle.Data->SetSetByCallerMagnitude(FWolfGameplayTags::Get().Data_Amount, -2.f);
		ASC->ApplyGameplayEffectSpecToSelf(*PassiveAdrenalineHandle.Data.Get());

		WOLF_LOG(Log, TEXT("Applied PassiveAdrenalineGE to %s"), *GetName());
	}

	// --- Passive Flow Gauge ---
	if (!PassiveFlowGaugeGE)
	{
		WOLF_WARN(TEXT("PassiveFlowGaugeGE is invalid for %s"), *GetName());
	}

	if (const auto PassiveFlowGaugeHandle = ASC->MakeOutgoingSpec(PassiveFlowGaugeGE, 1.f, ASC->MakeEffectContext());
		PassiveFlowGaugeHandle.IsValid())
	{
		PassiveFlowGaugeHandle.Data->SetSetByCallerMagnitude(FWolfGameplayTags::Get().Data_Amount, -2.f);
		ASC->ApplyGameplayEffectSpecToSelf(*PassiveFlowGaugeHandle.Data.Get());
	}
}

void AWolfCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasAuthority())
	{
		if (!IsValid(ASC))
		{
			SetupAbilitySystem();
		}
		if (IsValid(ASC))
		{
			ASC->InitAbilityActorInfo(this, this);
			ApplyDefaultAttributes();
			AddCharacterAbilities();
			WOLF_INFO(TEXT("Character %s possessed and GAS initialized."), *GetName());
		}
		else
		{
			WOLF_ERROR(TEXT("PossessedBy failed: ASC invalid for %s"), *GetName());
		}
	}
}

void AWolfCharacterBase::AddCharacterAbilities()
{
	if (!HasAuthority()) return;
	if (auto* WolfASC = Cast<UWolfAbilitySystemComponent>(ASC))
	{
		WolfASC->AddCharacterAbilities(StartupAbilities);
		WOLF_LOG(Log, TEXT("Added %d startup abilities to %s"), StartupAbilities.Num(), *GetName());
	}
	else
	{
		WOLF_WARN(TEXT("AddCharacterAbilities failed: ASC missing or not a WolfASC for %s"), *GetName());
	}
}

#pragma region Presage System

FTransform AWolfCharacterBase::GetProjectedTransform(float FutureTimeDelta) const
{
	FVector ProjectedLocation;
	const auto CurrentTransform = GetActorTransform();
	auto* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

	if (!AnimInstance || !AnimInstance->IsAnyMontagePlaying())
	{
		ProjectedLocation = GetActorLocation() + GetVelocity() * FutureTimeDelta;
		return FTransform(GetActorRotation(), ProjectedLocation, GetActorScale3D());
	}

	auto* CurrentMontage = AnimInstance->GetCurrentActiveMontage();

	if (!CurrentMontage || !CurrentMontage->HasRootMotion())
	{
		ProjectedLocation = GetActorLocation() + GetVelocity() * FutureTimeDelta;
		return FTransform(GetActorRotation(), ProjectedLocation, GetActorScale3D());
	}

	const auto CurrentMontagePosition = AnimInstance->Montage_GetPosition(CurrentMontage);
	const auto TargetMontagePosition = CurrentMontagePosition + FutureTimeDelta;

	const auto RootMotionDelta = CurrentMontage->ExtractRootMotionFromRange(CurrentMontagePosition,
	                                                                        TargetMontagePosition,
	                                                                        FAnimExtractContext());

	return RootMotionDelta * CurrentTransform;
}

float AWolfCharacterBase::GetTimeToNextHitImpact() const
{
	auto* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance) return -1.f;

	auto* Montage = AnimInstance->GetCurrentActiveMontage();
	if (!Montage) return -1.f;

	auto CurrentMontagePosition = AnimInstance->Montage_GetPosition(Montage);

	for (const auto& NotifyEvent : Montage->Notifies)
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
	if (auto* CurrentAbility = GetActiveCombatAbility())
	{
		return CurrentAbility->IsInvulnerableAt(RelativeTime);
	}

	return false;
}

class UBaseCombatAbility* AWolfCharacterBase::GetActiveCombatAbility() const
{
	if (!ASC) return nullptr;

	for (const auto& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive()) continue;

		for (auto* Instance : Spec.GetAbilityInstances())
		{
			if (auto* CombatAbility = Cast<UBaseCombatAbility>(Instance))
			{
				return CombatAbility;
			}
		}
	}
	return nullptr;

	/*TArray<FGameplayAbilitySpec> ActiveAbilities = ASC->GetActivatableAbilities();

	for (const auto& Spec : ActiveAbilities)
	{
		if (!Spec.IsActive()) continue;

		TArray<UGameplayAbility*> AbilityInstances = Spec.GetAbilityInstances();
		for (auto* AbilityInstance : AbilityInstances)
		{
			if (auto* TBAbility = Cast<UTBCombatAbility>(AbilityInstance)) return TBAbility;
		}
	}

	return nullptr;*/
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
	Snapshot.MovementMode = GetCharacterMovement()->MovementMode;
	Snapshot.CustomMovementMode = GetCharacterMovement()->CustomMovementMode;
}

void AWolfCharacterBase::SnapshotGAS(FActorSnapshot& Snapshot) const
{
	if (!ASC || !StatConfig) return;

	for (const auto& Pair : StatConfig->DefaultStats)
	{
		if (auto Attribute = UWolfAttributeSet::GetAttributeByTag(Pair.Key); Attribute.IsValid())
		{
			Snapshot.Attributes.Add(Attribute, ASC->GetNumericAttributeBase(Attribute));
		}
	}

	for (auto ActiveHandles = ASC->GetActiveEffects(FGameplayEffectQuery());
	     const auto& Handle : ActiveHandles)
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
	const auto* AnimInst = GetMesh()->GetAnimInstance();
	if (!AnimInst) return;

	if (auto* AnimMontage = AnimInst->GetCurrentActiveMontage())
	{
		Snapshot.CurrentMontage = AnimMontage;
		Snapshot.MontagePosition = AnimInst->Montage_GetPosition(AnimMontage);
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

	if (auto* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetComponentTickEnabled(true);
		MoveComp->Activate();

		MoveComp->SetMovementMode(Snapshot.MovementMode, Snapshot.CustomMovementMode);
		MoveComp->Velocity = Snapshot.Velocity;
		MoveComp->UpdateComponentVelocity();
	}

	if (auto* PrimitiveComp = Cast<UPrimitiveComponent>(GetRootComponent()))
	{
		if (!PrimitiveComp->IsSimulatingPhysics()) return;
		PrimitiveComp->SetPhysicsLinearVelocity(Snapshot.Velocity);
	}
}

void AWolfCharacterBase::RestoreGAS(const FActorSnapshot& Snapshot)
{
	if (!ASC) return;
	ASC->SetTagMapCount(FWolfGameplayTags::Get().InputState_Dead, 0);

	for (const TPair<FGameplayAttribute, float>& AttrPair : Snapshot.Attributes)
	{
		const auto& Attribute = AttrPair.Key;
		const auto AttributeValue = AttrPair.Value;

		if (FMath::IsNearlyEqual(ASC->GetNumericAttributeBase(Attribute), AttributeValue)) continue;
		ASC->SetNumericAttributeBase(Attribute, AttributeValue);
	}

	ASC->RemoveActiveEffects(FGameplayEffectQuery());
	for (const FStoredEffect& Effect : Snapshot.ActiveEffects)
	{
		if (!Effect.EffectClass) continue;

		const auto Context = ASC->MakeEffectContext();
		auto SpecHandle = ASC->MakeOutgoingSpec(Effect.EffectClass, Effect.Level, Context);

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
	if (auto* AnimInst = GetMesh()->GetAnimInstance())
	{
		AnimInst->StopAllMontages(0.f);
		if (!Snapshot.CurrentMontage.IsValid()) return;

		AnimInst->Montage_Play(Snapshot.CurrentMontage.Get(), 1.f);
		AnimInst->Montage_SetPosition(Snapshot.CurrentMontage.Get(), Snapshot.MontagePosition);
	}
}

#pragma endregion Presage System
