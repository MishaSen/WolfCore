// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/WolfEnemyBase.h"

#include "GameFramework/CharacterMovementComponent.h"


AWolfEnemyBase::AWolfEnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = nullptr;

	bUseControllerRotationYaw = false;
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bUseControllerDesiredRotation = true;
		GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
		GetCharacterMovement()->MaxWalkSpeed = 400.f;
	}
}

void AWolfEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void AWolfEnemyBase::Die_Implementation()
{
	Super::Die_Implementation();
}

