// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/WolfGameMode.h"

#include "LevelCombatMode.h"
#include "Core/WolfGameplayTags.h"
#include "Core/WolfPlayerController.h"
#include "Debug/WolfDebug.h"
#include "Systems/CombatModeSubsystem.h"

void AWolfGameMode::BeginPlay()
{
	Super::BeginPlay();

	auto* WolfPC = GetWolfPlayerController();
	UAbilitySystemComponent* ASC = WolfPC->GetASC();
	if (ASC)
	{
		const auto ReadyTag = FWolfGameplayTags::Get().Event_ModeSwitchReady;
		ASC->RegisterGameplayTagEvent(ReadyTag, EGameplayTagEventType::NewOrRemoved)
		.AddLambda([this, ASC, ReadyTag](const FGameplayTag, int32 NewCount)
		{
			if (ASC->HasMatchingGameplayTag(ReadyTag))
			{
				InitialModeSet();
			}
		});
	}
}

AWolfPlayerController* AWolfGameMode::GetWolfPlayerController()
{
	auto* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return nullptr;
	}

	auto* WPC = Cast<AWolfPlayerController>(PC);
	if (!WPC)
	{
		return nullptr;
	}

	return WPC;
}

void AWolfGameMode::InitialModeSet()
{
	if (auto* CombatModeSystem = GetWorld()->GetSubsystem<UCombatModeSubsystem>())
	{
		if (LevelCombatMode)
		{
			CombatModeSystem->SetCombatMode(LevelCombatMode->DefaultMode);
			WOLF_INFO(TEXT("SetCombatMode successful"));
		}
		else
		{
			WOLF_WARN(TEXT("No LevelCombatMode set for %s. Defaulting to combat mode OOC"), *GetWorld()->GetName());
			CombatModeSystem->SetCombatMode(ECombatMode::OOC);
		}
	}
	else
	{
		WOLF_ERROR(TEXT("Failed to get CombatModeSubsystem from world %s"), *GetWorld()->GetName());
	}
}
