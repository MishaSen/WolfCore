// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WolfCharacterBase.h"
#include "WolfEnemyBase.generated.h"

class UBehaviorTree;
class UWidgetComponent;

UCLASS()
class WOLFCORE_API AWolfEnemyBase : public AWolfCharacterBase
{
	GENERATED_BODY()

public:
	AWolfEnemyBase();

	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

protected:
	virtual void BeginPlay() override;

	virtual void Die_Implementation() override;
};
