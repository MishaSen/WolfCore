// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "WolfAIController.generated.h"

struct FAIStimulus;
class UAISenseConfig_Sight;

UCLASS()
class WOLFCORE_API AWolfAIController : public AAIController
{
	GENERATED_BODY()

public:
	AWolfAIController();

	UFUNCTION(BlueprintCallable, Category = "Wolf AI | Combat")
	void TryActivateAbilityByTag(FGameplayTag AbilityTag);

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wolf AI")
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wolf AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
};
