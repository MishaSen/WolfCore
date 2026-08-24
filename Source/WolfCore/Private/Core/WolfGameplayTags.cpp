// Fill out your copyright notice in the Description page of Project Settings.


#include "WolfCore/Public/Core/WolfGameplayTags.h"

#include "GameplayTagsManager.h"

FWolfGameplayTags FWolfGameplayTags::GameplayTags;

void FWolfGameplayTags::AddNativeTag(FGameplayTag& Tag, const FName& TagName, const FString& Description)
{
	Tag = UGameplayTagsManager::Get().AddNativeGameplayTag(TagName, Description);
}

void FWolfGameplayTags::InitializeNativeGameplayTags()
{
	// --- Input Tags ---
	AddNativeTag(GameplayTags.Input_Ability1, FName("Input.Ability1"), TEXT("Input tag for the first ability."));
	AddNativeTag(GameplayTags.Input_Ability2, FName("Input.Ability2"), TEXT("Input tag for the second ability."));
	AddNativeTag(GameplayTags.Input_Ability3, FName("Input.Ability3"), TEXT("Input tag for the third ability."));
	AddNativeTag(GameplayTags.Input_Ability4, FName("Input.Ability4"), TEXT("Input tag for the fourth ability."));
	AddNativeTag(GameplayTags.Input_Jump, FName("Input.Jump"), TEXT("Input tag for the jump ability."));
	AddNativeTag(GameplayTags.Input_Primary, FName("Input.Primary"), TEXT("Input tag for the primary ability."));
	AddNativeTag(GameplayTags.Input_Secondary, FName("Input.Secondary"), TEXT("Input tag for the secondary ability."));
	AddNativeTag(GameplayTags.Input_Switch, FName("Input.Switch"), TEXT("Input tag for the combat mode switch ability."));

	// --- Input State Tags ---
	AddNativeTag(GameplayTags.InputState, FName("InputState"), TEXT("Input state tag for all states."));
	AddNativeTag(GameplayTags.InputState_RT, FName("InputState.RT"), TEXT("Input state tag for RT state."));
	AddNativeTag(GameplayTags.InputState_TB, FName("InputState.TB"), TEXT("Input state tag for TB state."));
	AddNativeTag(GameplayTags.InputState_OOC, FName("InputState.OOC"), TEXT("Input state tag for OOC state."));
	AddNativeTag(GameplayTags.InputState_Dead, FName("InputState.Dead"), TEXT("Input state tag for dead state."));
	AddNativeTag(GameplayTags.InputState_Invulnerable, FName("InputState.Invulnerable"), TEXT("Input state tag for invulnerable state."));

	// --- Attributes ---
	AddNativeTag(GameplayTags.Attribute_Health, FName("Attribute.Health"), TEXT("Health attribute."));
	AddNativeTag(GameplayTags.Attribute_MaxHealth, FName("Attribute.MaxHealth"), TEXT("Max health attribute."));
	AddNativeTag(GameplayTags.Attribute_MaxFlowGauge, FName("Attribute.MaxFlowGauge"), TEXT("Max flow gauge attribute (soft cap on FlowGauge)."));
	AddNativeTag(GameplayTags.Attribute_FlowGauge, FName("Attribute.FlowGauge"), TEXT("Flow gauge attribute."));
	AddNativeTag(GameplayTags.Attribute_Adrenaline, FName("Attribute.Adrenaline"), TEXT("Adrenaline attribute."));

	// --- Effects ---
	AddNativeTag(GameplayTags.Effect_Combat, FName("Effect.Combat"), TEXT("Combat effect tag."));

	// --- Event Tags ---
	AddNativeTag(GameplayTags.Event_ModeSwitchReady, FName("Event.ModeSwitch"), TEXT("Event tag for mode switching."));
	AddNativeTag(GameplayTags.Event_Ability_Attack, FName("Event.Ability.Attack"), TEXT("Event tag for attacking."));

	// --- Presage Result ---
	AddNativeTag(GameplayTags.Result_Hit, FName("Result.Hit"), TEXT("Result tag for a hit."));
	AddNativeTag(GameplayTags.Result_Dodge, FName("Result.Dodge"), TEXT("Result tag for a dodge."));

	// --- Data ---
	AddNativeTag(GameplayTags.Data_Amount, FName("Data.Amount"), TEXT("Data tag for amount."));
	AddNativeTag(GameplayTags.Data_FlowAmount, FName("Data.FlowAmount"), TEXT("Data tag for Flow Gauge gain amount."));
	AddNativeTag(GameplayTags.Data_AdrenalineAmount, FName("Data.AdrenalineAmount"), TEXT("Data tag for Adrenaline gain amount."));

	// --- Status ---
	AddNativeTag(GameplayTags.Status_Link, FName("Status.Link"), TEXT("Status tag for link."));
	AddNativeTag(GameplayTags.Status_Airborne, FName("Status.Airborne"), TEXT("Status tag for airborne — reaction window for conditional hit effects."));
}