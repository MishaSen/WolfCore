// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ActorState.generated.h"

class AActor;
class UAnimSequence;
class UAnimMontage;

USTRUCT(BlueprintType)
struct FActorState
{
	GENERATED_BODY()
	FActorState()
		: ActorRef(nullptr)
		, Location(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, Velocity(FVector::ZeroVector)
		, MovementMode(EMovementMode::MOVE_None)
		, CustomMovementMode(0)
		, CurrentMontage(nullptr)
		, MontagePosition(0.0f)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorState")
	TObjectPtr<AActor> ActorRef;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorState")
	FVector Location;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorState")
	FRotator Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorState")
	FVector Velocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorState")
	TEnumAsByte<EMovementMode> MovementMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorState")
	uint8 CustomMovementMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorState")
	TWeakObjectPtr<UAnimMontage> CurrentMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorState")
	float MontagePosition;
};