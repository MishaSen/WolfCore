// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCore/Public/Character/WolfCharacterBase.h"

#include "Abilities/AbilityConfig.h"
#include "Components/CapsuleComponent.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"
#include "WolfCore/Public/AbilitySystem/WolfAttributeSet.h"
#include "WolfCore/Public/Presage/PresageAbilityRequest.h"
#include "WolfCore/Public/Presage/PresageSubsystem.h"

AWolfCharacterBase::AWolfCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponentClass = UWolfAbilitySystemComponent::StaticClass();
	AttributeSet = CreateDefaultSubobject<UWolfAttributeSetBase>("AttributeSet");

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

	if (HasAuthority() && !IsValid(ASC))
	{
		SetupAbilitySystem();
	}
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
	if (IsValid(ASC) && DefaultAttributes)
	{
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		if (const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(DefaultAttributes, 1, EffectContext);
			SpecHandle.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			WOLF_LOG(Log, TEXT("Applied DefaultAttributesSpec to %s"), *GetName());
		}
		else
		{
			WOLF_WARN(TEXT("Failed to apply DefaultAttributesSpec to %s"), *GetName());
		}
	}
	else
	{
		WOLF_WARN(TEXT("ApplyDefaultAttributes called on %s but ASC or DefaultAttributes invalid."), *GetName());
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
	auto CurrentTransform = GetActorTransform();
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

	auto CurrentMontagePosition = AnimInstance->Montage_GetPosition(CurrentMontage);
	auto TargetMontagePosition = CurrentMontagePosition + FutureTimeDelta;

	auto RootOffset = ExtractRootMotionAtTime(CurrentMontage, TargetMontagePosition);

	return RootOffset * CurrentTransform;
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
	auto* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		OutRadius = 0.f;
		OutHalfHeight = 0.f;
		return;
	}

	OutRadius = Capsule->GetScaledCapsuleRadius();
	OutHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
}

FTransform AWolfCharacterBase::ExtractRootMotionAtTime(UAnimMontage* Montage, float Time) const
{
	if (!Montage) return FTransform::Identity;

	return Montage->ExtractRootMotionFromRange(0.f, Time);
}

#pragma endregion Presage System