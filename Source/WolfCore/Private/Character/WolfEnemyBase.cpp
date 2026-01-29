// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/WolfEnemyBase.h"

#include "AIController.h"
#include "Character/AI/WolfAIController.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"

AWolfEnemyBase::AWolfEnemyBase()
{
	AIControllerClass = AWolfAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->MaxWalkSpeed = 400.f;
}

void AWolfEnemyBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	auto* AIController = Cast<AWolfAIController>(NewController);
	if (!IsValid(AIController))
	{
		WOLF_WARN(TEXT("AI Controller on %s is not valid."), *GetName());
		return;
	}

	if (BehaviorTree)
	{
		AIController->RunBehaviorTree(BehaviorTree);
	}
}