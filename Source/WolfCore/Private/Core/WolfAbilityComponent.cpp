// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/WolfAbilityComponent.h"

#include "AbilitySystem/WolfAttributeSet.h"
#include "Debug/WolfDebug.h"
#include "AbilitySystem/CharacterStatConfig.h"


// Sets default values for this component's properties
UWolfAbilityComponent::UWolfAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWolfAbilityComponent::InitializeAbilitySystem(AActor* InOwner)
{
	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = NewObject<UWolfAbilitySystemComponent>(InOwner, TEXT("ASC"));
		AbilitySystemComponent->SetIsReplicated(false);
		AbilitySystemComponent->RegisterComponent();

		AttributeSet = NewObject<UWolfAttributeSet>(InOwner, TEXT("AttributeSet"));

		WOLF_LOG(Log, TEXT("AbilitySystemComponent initialized for %s."), *InOwner->GetName());
	}

	if (IsValid(StatConfig))
	{
		CachedAttributes.Empty();
		for (const auto& Pair : StatConfig->DefaultStats)
		{
			auto Attribute = UWolfAttributeSet::GetAttributeByTag(Pair.Key);
			if (Attribute.IsValid()) CachedAttributes.Add(Attribute);
		}
	}
}

void UWolfAbilityComponent::ApplyDefaultAttributes()
{
	if (!IsValid(AbilitySystemComponent) || !IsValid(DefaultAttributes) || !IsValid(StatConfig)) return;

	auto EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(GetOwner());

	const auto SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributes, 1, EffectContext);
	if (!SpecHandle.IsValid()) return;

	for (const auto& [Tag, Value] : StatConfig->DefaultStats)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(Tag, Value);

		if (!UWolfAttributeSet::GetAttributeByTag(Tag).IsValid())
		{
			WOLF_WARN(TEXT("Attribute Tag [%s] is in StatConfig but NOT registered in UWolfAttributeSet mapping!"
						   "Snapshots/Presage will ignore this stat."), *Tag.ToString());
		}
	}
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	WOLF_LOG(Log, TEXT("Applied all attributes from StatConfig to %s"), *GetOwner()->GetName());
}

void UWolfAbilityComponent::AddStartupAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities)
{
	if (GetOwnerRole() != ROLE_Authority || !IsValid(AbilitySystemComponent)) return;
	
	AbilitySystemComponent->AddCharacterAbilities(Abilities);
	WOLF_LOG(Log, TEXT("Added %d startup abilities to %s"), Abilities.Num(), *GetOwner()->GetName());
}