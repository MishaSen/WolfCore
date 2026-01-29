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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	uint8 TeamID = 2;

protected:
	virtual void PossessedBy(AController* NewController) override;
};
