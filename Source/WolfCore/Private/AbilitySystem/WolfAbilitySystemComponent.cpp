// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/AbilitySystem/WolfAbilitySystemComponent.h"

void UWolfAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& Tag)
{
	if (!Tag.IsValid()) return;

	const FPredictionKey PredictionKey = FPredictionKey::CreateNewPredictionKey(this);
	ServerSetInputTagPressed(Tag, PredictionKey);
	
	FScopedPredictionWindow ScopedPredictionWindow(this, PredictionKey);
	FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Tag))
		{
			AbilitySpecInputPressed(AbilitySpec);
			if (AbilitySpec.IsActive())
			{
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed,
				                      AbilitySpec.Handle,
				                      PredictionKey);
			}
		}
	}
}

void UWolfAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& Tag)
{
	if (!Tag.IsValid()) return;

	const FPredictionKey PredictionKey = FPredictionKey::CreateNewPredictionKey(this);
	ServerSetInputTagReleased(Tag, PredictionKey);

	FScopedPredictionWindow ScopedPredictionWindow(this, PredictionKey);
	FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Tag) && AbilitySpec.IsActive())
		{
			AbilitySpecInputReleased(AbilitySpec);
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased,
								  AbilitySpec.Handle,
								  PredictionKey);
		}
	}
}

void UWolfAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& Tag)
{

	if (!Tag.IsValid()) return;

	const FPredictionKey PredictionKey = FPredictionKey::CreateNewPredictionKey(this);
	ServerSetInputTagHeld(Tag, PredictionKey);

	FScopedPredictionWindow ScopedPredictionWindow(this, PredictionKey);
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

// --- Server RPCs --- (Get key from client)
void UWolfAbilitySystemComponent::ServerSetInputTagPressed_Implementation(FGameplayTag Tag, FPredictionKey PredictionKey) {}
void UWolfAbilitySystemComponent::ServerSetInputTagReleased_Implementation(FGameplayTag Tag, FPredictionKey PredictionKey) {}
void UWolfAbilitySystemComponent::ServerSetInputTagHeld_Implementation(FGameplayTag Tag, FPredictionKey PredictionKey) {}

// --- Validation Functions ---
bool UWolfAbilitySystemComponent::ServerSetInputTagPressed_Validate(FGameplayTag Tag, FPredictionKey PredictionKey) { return true; }
bool UWolfAbilitySystemComponent::ServerSetInputTagReleased_Validate(FGameplayTag Tag, FPredictionKey PredictionKey) { return true; }
bool UWolfAbilitySystemComponent::ServerSetInputTagHeld_Validate(FGameplayTag Tag, FPredictionKey PredictionKey) { return true; }



