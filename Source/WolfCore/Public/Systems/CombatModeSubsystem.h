// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "Interfaces/CombatModeListener.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatModeSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class WOLFCORE_API UCombatModeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void SetMode(FGameplayTag NewMode);
	void SwitchCombatMode();

	FGameplayTag GetCombatMode() const { return CurrentMode; }

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void RegisterCombatListener(AActor* Combatant);
	void UnregisterCombatListener(const AActor* Combatant);
	void ApplyModeToActor(AActor* Combatant, FGameplayTag NewMode);
	void UpdateCombatantModeTags(FGameplayTag NewMode);

	FGameplayTag GetCurrentMode() const { return CurrentMode; }

private:
	FGameplayTag CurrentMode;
	FWolfGameplayTags WolfTag;

	UPROPERTY()
	TSet<AActor*> Combatants;

	UAbilitySystemComponent* GetPlayerASC() const;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Settings")
	TMap<FGameplayTag, float> ModeTimeDilationMap; // Possibly implement as a data asset in the future

	FORCEINLINE float GetDilationForMode(const FGameplayTag& Mode) const
	{
		if (const auto* FoundDilation = ModeTimeDilationMap.Find(Mode))
		{
			return *FoundDilation;
		}
		return 1.f;
	}
};
