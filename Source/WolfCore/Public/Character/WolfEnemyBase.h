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
	/** Default constructor for AWolfEnemyBase, initializing enemy-specific AI components. */
	AWolfEnemyBase();

	/** Probability [0,1] that this combatant defers its Presage-planning decision to round 2 instead
	  * of committing immediately in round 1. Higher values behave more "reactively" — they wait to
	  * see what other combatants declare before picking their own action. See
	  * FPresageOrchestrator::RunPlanning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|AI", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float ReactiveDispositionWeight = 0.3f;

protected:
	/** Handles possession by a new controller, setting up behavior tree and AI state accordingly. Overrides ACharacter::PossessedBy(). */
	/**
	 * @param NewController The controller that has taken possession of this enemy character.
	 */
	virtual void PossessedBy(AController* NewController) override;

	// ============================================================================================================================
	// AI Configuration
	// ============================================================================================================================

	/** Behavior tree asset defining the decision-making logic and behavior patterns for this enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	/** Team identifier used for ally/enemy grouping and team-based combat interactions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WolfCore|AI")
	uint8 TeamID = 2;
};
