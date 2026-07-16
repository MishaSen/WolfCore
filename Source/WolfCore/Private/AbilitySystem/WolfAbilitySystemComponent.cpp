// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"

#include "Abilities/TBCombatAbility.h"
#include "Character/WolfCharacterBase.h"
#include "Core/WolfGameplayTags.h"
#include "Core/WolfPresageComponent.h"
#include "Debug/WolfDebug.h"
#include "Systems/CombatModeSubsystem.h"

bool UWolfAbilitySystemComponent::TryInjectPresageAbility(const FGameplayTag& Tag)
{
	auto* Avatar = Cast<AWolfCharacterBase>(GetAvatarActor());
	if (!Avatar) return false;

	auto* CMS = Avatar->GetCMS();
	if (!CMS || !CMS->bIsInTB) return false;

	const auto Request = BuildInitialPresageRequest(Tag, Avatar->GatherPresageTargets());
	if (!Request.AbilityClass) return false;

	if (auto* Presage = Avatar->GetPresageComponent())
	{
		Presage->SetInjectedAbilityRequest(Request);
		CMS->ReBakeTimeline();
		WOLF_LOG(Log, TEXT("Presage injected %s for %s"), *Tag.ToString(), *Avatar->GetName());
		return true;
	}

	return false;
}

void UWolfAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& Tag)
{
	if (!Tag.IsValid()) return;

	if (TryInjectPresageAbility(Tag)) return;

	FScopedAbilityListLock ActiveScopeLock(*this);
	for (auto& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Tag))
		{
			AbilitySpecInputPressed(AbilitySpec);

			const bool bIsPlayerControlled = GetAvatarActor() ? Cast<APawn>(GetAvatarActor())->IsPlayerControlled() : false; 
			if (!bIsPlayerControlled && !AbilitySpec.IsActive()) // For AI activated abilities
			{
				TryActivateAbility(AbilitySpec.Handle);
			}
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

	if (TryInjectPresageAbility(Tag)) return;

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

TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> UWolfAbilitySystemComponent::AddCharacterAbilities(TArray<TSubclassOf<UGameplayAbility>> StartupAbilities)
{
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> OutHandles;

	for (const auto AbilityClass : StartupAbilities)
	{
		auto AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		if (const auto* WolfAbility = Cast<UBaseCombatAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(WolfAbility->StartupInputTag);
			FGameplayAbilitySpecHandle Handle = GiveAbility(AbilitySpec);
			OutHandles.Add(AbilityClass, Handle);

			WOLF_LOG(Log, TEXT("Added ability %s for tag %s"), *AbilitySpec.Ability->GetName(),
			         *WolfAbility->StartupInputTag.ToString());
		}
	}
	return OutHandles;
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
			TBAbility->GetAbilitySequence(),
			Targets
		);
	}

	return FPresageAbilityRequest();
}