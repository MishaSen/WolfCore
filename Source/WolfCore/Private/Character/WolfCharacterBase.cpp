// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCore/Public/Character/WolfCharacterBase.h"

#include "Abilities/AbilityConfig.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"
#include "WolfCore/Public/AbilitySystem/WolfAttributeSet.h"
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

void AWolfCharacterBase::QueueAbility(const FGameplayAbilitySpecHandle SpecHandle, const float ScheduledTime,
                                      const FGameplayTag AbilityTag)
{
	if (!IsValid(ASC)) return;

	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(SpecHandle);
	if (!Spec || !Spec->Ability)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Ability or Spec Handle"));
		return;
	}
	const FPresageAbilityRequest Request(ASC, SpecHandle, ScheduledTime, AbilityTag);

	if (UWorld* World = GetWorld())
	{
		if (UPresageSubsystem* Presage = UPresageSubsystem::Get(World))
		{
			Presage->QueueAbilityRequest(Request);
			UE_LOG(LogTemp, Log, TEXT("Queued ability at %s at time %f"),
			       *Spec->Ability->GetClass()->GetName(),
			       ScheduledTime)
		}
	}
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
		UE_LOG(LogTemp, Error, TEXT("No AbilitySystemComponentClass set for %s"), *GetName());
		return;
	}

	ASC = NewObject<UWolfAbilitySystemComponent>(this, AbilitySystemComponentClass, TEXT("ASC"));
	if (ASC)
	{
		ASC->SetIsReplicated(false);
		ASC->RegisterComponent();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create AbilitySystemComponent for %s"), *GetName());
	}
}

void AWolfCharacterBase::ApplyDefaultAttributes()
{
	if (IsValid(ASC) && DefaultAttributes)
	{
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		if (const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
			DefaultAttributes,
			1,
			EffectContext
		); SpecHandle.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			UE_LOG(LogTemp, Log, TEXT("Applied DefaultAttributes to character: %s"), *GetName());
		}
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
			UE_LOG(LogTemp, Log, TEXT("Character %s possessed and GAS initialized."), *GetName());
		}
	}
}

void AWolfCharacterBase::AddCharacterAbilities()
{
	UWolfAbilitySystemComponent* WolfASC = Cast<UWolfAbilitySystemComponent>(ASC);
	if (!HasAuthority()) return;

	WolfASC->AddCharacterAbilities(StartupAbilities);
}
