// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/AI/WolfAIController.h"

#include "AbilitySystem/WolfAbilitySystemComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/WolfCharacterBase.h"
#include "Debug/WolfDebug.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"


// Sets default values
AWolfAIController::AWolfAIController()
{
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->SightRadius = 1500.f;
	SightConfig->LoseSightRadius = 2000.f;
	SightConfig->PeripheralVisionAngleDegrees = 90.f;

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
}

void AWolfAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	const auto* WolfChar = Cast<AWolfCharacterBase>(InPawn);
	if (WolfChar)
	{
		AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AWolfAIController::OnPerceptionUpdated);
	}
}

void AWolfAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed()) return;

	auto* BlackComp = GetBlackboardComponent();
	if (!BlackComp)
	{
		WOLF_WARN(TEXT("BlackboardComponent not found on %s"), *GetName());
		return;
	}

	BlackComp->SetValueAsObject(FName("TargetActor"), Actor);
}

void AWolfAIController::TryActivateAbilityByTag(FGameplayTag AbilityTag)
{
	const auto WolfChar = Cast<AWolfCharacterBase>(GetPawn());
	if (!WolfChar) return;

	auto* ASC = Cast<UWolfAbilitySystemComponent>(WolfChar->GetAbilitySystemComponent());
	if (ASC)
	{
		ASC->AbilityInputTagPressed(AbilityTag);
	}
}

