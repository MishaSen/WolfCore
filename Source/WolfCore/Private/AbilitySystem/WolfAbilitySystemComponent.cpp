// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"

#include "Abilities/TBCombatAbility.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"

void UWolfAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& Tag)
{
	if (!Tag.IsValid()) return;

	FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Tag))
		{
			AbilitySpecInputPressed(AbilitySpec);
		}
	}
}

void UWolfAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& Tag)
{
	if (!Tag.IsValid()) return;

	FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Tag) && AbilitySpec.IsActive())
		{
			AbilitySpecInputReleased(AbilitySpec);
		}
	}
}

void UWolfAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& Tag)
{
	if (!Tag.IsValid()) return;

	FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Tag))
		{
			AbilitySpecInputPressed(AbilitySpec);
			if (!AbilitySpec.IsActive())
			{
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
	}
}

void UWolfAbilitySystemComponent::AddCharacterAbilities(TArray<TSubclassOf<UGameplayAbility>> StartupAbilities)
{
	for (const auto AbilityClass : StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		if (const UBaseCombatAbility* WolfAbility = Cast<UBaseCombatAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(WolfAbility->StartupInputTag);
			GiveAbility(AbilitySpec);
			WOLF_LOG(Log, TEXT("Added ability %s for tag %s"), *AbilitySpec.Ability->GetName(), *WolfAbility->StartupInputTag.ToString());
		}
	}
}

FPresageAbilityRequest UWolfAbilitySystemComponent::BuildInitialPresageRequest(const FGameplayTag& Tag,
                                                                               const TArray<TWeakObjectPtr<AActor>>&
                                                                               Targets)
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(Tag))
		{
			if (const UTBCombatAbility* TBAbility = Cast<UTBCombatAbility>(Spec.Ability))
			{
				return FPresageAbilityRequest(
					this,
					Tag,
					0.f,
					TBAbility->AbilitySequence,
					Targets
				);
			}
		}
	}
	return FPresageAbilityRequest();
}
