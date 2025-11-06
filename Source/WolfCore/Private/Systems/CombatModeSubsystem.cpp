// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/CombatModeSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"

void UCombatModeSubsystem::SetCombatMode(ECombatMode NewMode)
{
	if (NewMode == CurrentMode)
	{
		WOLF_INFO(TEXT("CombatMode already set to %s"), *UEnum::GetValueAsString(NewMode));
	}

	CurrentMode = NewMode;
	ApplyModeToASC(NewMode);

	WOLF_INFO(TEXT("CombatMode set to %s"), *UEnum::GetValueAsString(NewMode));
}

void UCombatModeSubsystem::ApplyModeToASC(ECombatMode Mode)
{
	auto* ASC = GetPlayerASC();
	if (!ASC)
	{
		WOLF_WARN(TEXT("No ASC found for player"));
		return;
	}

	for (const auto& Tags = FWolfGameplayTags::Get();
	     const auto& TagToRemove : {Tags.InputState_RT, Tags.InputState_TB, Tags.InputState_OOC})
	{
		ASC->RemoveLooseGameplayTag(TagToRemove);
	}

	if (const auto TagToAdd = GetTagForMode(Mode); TagToAdd.IsValid())
	{
		ASC->AddLooseGameplayTag(TagToAdd);
		WOLF_LOG(Log, TEXT("Applied mode tag %s to ASC"), *TagToAdd.ToString());
	}
}

UAbilitySystemComponent* UCombatModeSubsystem::GetPlayerASC() const
{
	if (!GetWorld())
	{
		WOLF_WARN(TEXT("Invalid World"));
		return nullptr;
	}

	if (const auto* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
		}
	}
	WOLF_WARN(TEXT("GetPlayerASC returned nullptr"));
	return nullptr;
}

FGameplayTag UCombatModeSubsystem::GetTagForMode(ECombatMode Mode) const
{
	const FWolfGameplayTags& Tags = FWolfGameplayTags::Get();
	switch (Mode)
	{
	case ECombatMode::RT: return Tags.InputState_RT;
	case ECombatMode::TB: return Tags.InputState_TB;
	case ECombatMode::OOC: return Tags.InputState_OOC;
	default: return FGameplayTag();
	}
}
