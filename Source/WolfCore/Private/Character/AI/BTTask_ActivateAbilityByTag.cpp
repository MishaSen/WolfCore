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
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_ActivateAbilityByTag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	auto* ASC = GetASC(OwnerComp);
	if (!ASC || !AbilityTag.IsValid()) return EBTNodeResult::Failed;

	CachedOwnerBTComp = &OwnerComp;

	if (bWaitForCompletion) // Instant abilities edge case
	{
		ASC->OnAbilityEnded.AddUObject(this, &UBTTask_ActivateAbilityByTag::OnAbilityEnded);
	}
	
	ASC->AbilityInputTagPressed(AbilityTag);
	WOLF_INFO( TEXT( "Ability [%s] activated." ), *AbilityTag.ToString());
	
	return bWaitForCompletion ? EBTNodeResult::InProgress : EBTNodeResult::Succeeded;
}

void UBTTask_ActivateAbilityByTag::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
	EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

	if (auto* ASC = GetASC(OwnerComp))
	{
		ASC->OnAbilityEnded.RemoveAll(this);

		ASC->AbilityInputTagReleased(AbilityTag);
	}

	CachedOwnerBTComp.Reset();
}

void UBTTask_ActivateAbilityByTag::OnAbilityEnded(const FAbilityEndedData& AbilityEndedData)
{
	const auto EndedAbility = Cast<UBaseCombatAbility>(AbilityEndedData.AbilityThatEnded);
	if (!CachedOwnerBTComp.IsValid() || !EndedAbility) return;

	if (EndedAbility->StartupInputTag == AbilityTag)
	{
		const auto Result = AbilityEndedData.bWasCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
		FinishLatentTask(*CachedOwnerBTComp, Result);
	}
}

UWolfAbilitySystemComponent* UBTTask_ActivateAbilityByTag::GetASC(const UBehaviorTreeComponent& OwnerBTComp)
{
	const auto* AIController = OwnerBTComp.GetAIOwner();
	const auto* MyPawn = Cast<AWolfCharacterBase>(AIController ? AIController->GetPawn() : nullptr);
	return MyPawn ? Cast<UWolfAbilitySystemComponent>(MyPawn->GetAbilitySystemComponent()) : nullptr;
}
