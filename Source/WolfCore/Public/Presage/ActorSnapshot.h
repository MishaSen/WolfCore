// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "ActorSnapshot.generated.h"

class AActor;
class UAnimMontage;
class UBaseCombatAbility;
class UBTNode;
class UGameplayEffect;

// ============================================================================================================================
// Simulation Configuration Constants
// ============================================================================================================================

/** Namespace containing simulation configuration constants for temporal prediction and presage tick rates. */
namespace WolfSimConfig
{
	/** Simulation frequency in Hz defining the number of ticks per second during presage prediction. */
	static constexpr float Frequency = 10.f;

	/** Fixed time step in seconds between each simulation tick during temporal prediction. */
	static constexpr float Step = 0.1f;
}

// ============================================================================================================================
// Stored Gameplay Effect
// ============================================================================================================================

/**
 * Container for a Gameplay Effect to be applied during presage simulation.
 */
USTRUCT(BlueprintType)
struct FStoredEffect
{
	GENERATED_BODY()

	/** Subclass of GameplayEffect that will be applied to the actor during presage simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Effects")
	TSubclassOf<UGameplayEffect> EffectClass;

	/** Effect level value used when applying this stored gameplay effect during simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Effects")
	float Level = 1.f;

	/** Number of effect stacks to apply when spawning this gameplay effect instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Effects")
	int32 Stacks = 1;

	/** Remaining duration in seconds for the stored gameplay effect after application. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Effects")
	float RemainingDuration = -1.f;
};

// ============================================================================================================================
// Actor State Snapshot
// ============================================================================================================================

/**
 * Complete snapshot of an actor's state at a point in time for presage simulation.
 * Captures transform, movement, combat state, animation, attributes, effects, and AI behavior.
 */
USTRUCT(BlueprintType)
struct FActorSnapshot
{
	GENERATED_BODY()

	FActorSnapshot() = default;

	// ============================================================================================================================
	// Transform & Movement
	// ============================================================================================================================

	/** Weak reference to the actor whose state is being captured in this snapshot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	TWeakObjectPtr<AActor> ActorRef = nullptr;

	/** World-space location vector representing the actor's position at the time of capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	FVector Location = FVector::ZeroVector;

	/** World-space rotation value representing the actor's orientation at the time of capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	FRotator Rotation = FRotator::ZeroRotator;

	/** World-space velocity vector representing the actor's movement speed and direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	FVector Velocity = FVector::ZeroVector;

	/** Enumerated movement mode (e.g., None, Fall, Swim, Jump) indicating current locomotion state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	TEnumAsByte<EMovementMode> MovementMode = MOVE_None;

	/** Custom movement mode identifier for specialized locomotion behaviors beyond standard movement modes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	uint8 CustomMovementMode = 0;

	// ============================================================================================================================
	// Combat State
	// ============================================================================================================================

	/** Weak reference to the currently active combat ability being executed during this snapshot period. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State|Combat")
	TWeakObjectPtr<UBaseCombatAbility> ActiveAbility = nullptr;

	/** Current index within the combat ability's sequence indicating which period is actively executing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State|Combat")
	int32 CurrentPeriodIndex = 0;

	// ============================================================================================================================
	// Animation
	// ============================================================================================================================

	/** Weak reference to the animation montage currently playing during this snapshot period. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	TWeakObjectPtr<UAnimMontage> CurrentMontage = nullptr;

	/** Playback position in seconds within the current animation montage at capture time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	float MontagePosition = 0.f;

	// ============================================================================================================================
	// Attributes & Effects
	// ============================================================================================================================

	/** Array of floating-point values representing captured attribute data from the character stat configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	TArray<float> AttributeValues; // Use Attributes from StatConfig

	/** Array of stored gameplay effects to be applied during presage simulation with their parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	TArray<FStoredEffect> ActiveEffects;

	/** Container of GameplayTags representing the actor's current state flags and active modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	FGameplayTagContainer Tags;

	// ============================================================================================================================
	// AI Behavior
	// ============================================================================================================================

	/** Weak reference to the behavior tree node currently executing during this snapshot period. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	TWeakObjectPtr<UBTNode> ActiveNode = nullptr; // We might want to see what node was running

	// ============================================================================================================================
	// Navigation Target
	// ============================================================================================================================

	/** Weak reference to the target actor being pursued or tracked during this snapshot period. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	TWeakObjectPtr<AActor> TargetActor = nullptr;

	/** World-space destination vector representing the navigation target location for movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	FVector Destination = FVector::ZeroVector;

	/** Boolean flag indicating whether the actor is currently in motion toward its navigation destination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfCore|Actor State")
	bool bIsMoving = false;
};

// ============================================================================================================================
// Temporal State Container
// ============================================================================================================================

/**
 * Maps actors to their snapshots at a specific world time anchor for temporal prediction queries.
 */
USTRUCT(BlueprintType)
struct FTemporalStates
{
	GENERATED_BODY()

	/** World-space time anchor value in seconds serving as the reference point for snapshot queries. */
	UPROPERTY()
	float WorldTimeAnchor = 0.f;

	/** Map of actor references to their corresponding snapshots at the specified world time anchor. */
	UPROPERTY()
	TMap<const AActor*, FActorSnapshot> ActorStates;
};