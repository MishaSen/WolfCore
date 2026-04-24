// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WolfCharacterBase.h"
#include "WolfEnemyBase.generated.h"

// ============================================================================================================================
// Forward Declarations
// ============================================================================================================================

class UBehaviorTree;

/**
 * Base enemy character with AI behavior tree integration.
 */
UCLASS()
class WOLFCORE_API AWolfEnemyBase : public AWolfCharacterBase
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	AWolfEnemyBase();

protected:
	virtual void PossessedBy(AController* NewController) override;

	// ============================================================================================================================
	// AI Configuration
	// ============================================================================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	uint8 TeamID = 2;
};