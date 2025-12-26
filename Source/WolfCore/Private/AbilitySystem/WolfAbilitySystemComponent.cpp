// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"

#include "Abilities/TBCombatAbility.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"

void UWolfAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& Tag)
{
	if (!Tag.IsValid()) return;

	FScopedAbilityListLock ActiveScopeLock(*this);
	for (auto& AbilitySpec : GetActivatableAbilities())
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
	for (auto& AbilitySpec : GetActivatableAbilities())
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
	for (auto& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Tag)) continue;

		AbilitySpecInputPressed(AbilitySpec);
		if (!AbilitySpec.IsActive())
		{
			TryActivateAbility(AbilitySpec.Handle);
		}
	}
}

void UWolfAbilitySystemComponent::AddCharacterAbilities(TArray<TSubclassOf<UGameplayAbility>> StartupAbilities)
{
	for (const auto AbilityClass : StartupAbilities)
	{
		auto AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		if (const auto* WolfAbility = Cast<UBaseCombatAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(WolfAbility->StartupInputTag);
			GiveAbility(AbilitySpec);

			WOLF_LOG(Log, TEXT("Added ability %s for tag %s"), *AbilitySpec.Ability->GetName(),
			         *WolfAbility->StartupInputTag.ToString());
		}
	}
}

FPresageAbilityRequest UWolfAbilitySystemComponent::BuildInitialPresageRequest(const FGameplayTag& Tag,
                                                                               const TArray<TWeakObjectPtr<AActor>>&
                                                                               Targets)
{
	for (const auto& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Tag)) continue;

		const auto* TBAbility = Cast<UTBCombatAbility>(AbilitySpec.Ability);
		if (!TBAbility) continue;

		return FPresageAbilityRequest(
			TBAbility->GetClass(),
			this,
			Tag,
			0.f,
			TBAbility->AbilitySequence,
			Targets
		);
	}

	return FPresageAbilityRequest();
}