// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/AI/BTTask_ActivateAbilityByTag.h"

#include "AIController.h"
#include "AbilitySystem/WolfAbilitySystemComponent.h"
#include "Character/WolfCharacterBase.h"
#include "Debug/WolfDebug.h"

UBTTask_ActivateAbilityByTag::UBTTask_ActivateAbilityByTag()
{
	NodeName = TEXT("Activate Ability By Tag");
	bNotifyTick = false;
}

EBTNodeResult::Type UBTTask_ActivateAbilityByTag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const auto* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	const auto* MyPawn = Cast<AWolfCharacterBase>(AIController->GetPawn());
	if (!MyPawn) return EBTNodeResult::Failed;

	auto* ASC = Cast<UWolfAbilitySystemComponent>(MyPawn->GetAbilitySystemComponent());
	if (!ASC || !AbilityTag.IsValid()) return EBTNodeResult::Failed;

	ASC->AbilityInputTagPressed(AbilityTag);
	WOLF_INFO( TEXT( "Ability [%s] activated." ), *AbilityTag.ToString());
	return EBTNodeResult::Succeeded;
}