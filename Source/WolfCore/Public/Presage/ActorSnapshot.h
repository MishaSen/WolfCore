// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/BaseCombatAbility.h"
#include "Engine/EngineTypes.h"

#include "ActorSnapshot.generated.h"

class UBTNode;
class UGameplayEffect;
class AActor;
class UAnimSequence;
class UAnimMontage;

namespace WolfSimConfig
{
	static constexpr float Frequency = 10.f;
	static constexpr float Step = 0.1f;
}

USTRUCT(BlueprintType)
struct FStoredEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> EffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Level = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Stacks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RemainingDuration = -1.f;
};

USTRUCT(BlueprintType)
struct FActorSnapshot
{
	GENERATED_BODY()
	
	FActorSnapshot() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	TWeakObjectPtr<AActor> ActorRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	TEnumAsByte<EMovementMode> MovementMode = MOVE_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	uint8 CustomMovementMode = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State|Combat")
	TWeakObjectPtr<UBaseCombatAbility> ActiveAbility = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State|Combat")
	int32 CurrentPeriodIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	TWeakObjectPtr<UAnimMontage> CurrentMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	float MontagePosition = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	TArray<float> AttributeValues; // Use Attributes from StatConfig

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	TArray<FStoredEffect> ActiveEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	FGameplayTagContainer Tags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	TWeakObjectPtr<UBTNode> ActiveNode = nullptr; // We might want to see what node was running

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	TWeakObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	FVector Destination = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor State")
	bool bIsMoving = false;
};

USTRUCT(BlueprintType)
struct FTemporalStates
{
	GENERATED_BODY()

	UPROPERTY()
	float WorldTimeAnchor = 0.f;

	UPROPERTY()
	TMap<const AActor*, FActorSnapshot> ActorStates;
};