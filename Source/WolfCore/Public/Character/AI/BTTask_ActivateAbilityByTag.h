// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "BTTask_ActivateAbilityByTag.generated.h"

class UWolfAbilitySystemComponent;
class UGameplayAbility;
/**
 * 
 */
UCLASS()
class WOLFCORE_API UBTTask_ActivateAbilityByTag : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ActivateAbilityByTag();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	UPROPERTY()
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerBTComp = nullptr;

	void OnAbilityEnded(const FAbilityEndedData& AbilityEndedData);

	UPROPERTY(EditAnywhere, Category = "Wolf AI")
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, Category = "Wolf AI")
	bool bWaitForCompletion = true;

private:
	static UWolfAbilitySystemComponent* GetASC(const UBehaviorTreeComponent& OwnerBTComp);
};