#pragma once

#include "CoreMinimal.h"
#include "Presage/ActorSnapshot.h"

#include "WolfTemporalStates.generated.h"

USTRUCT(BlueprintType)
struct FWolfTemporalStates
{
	GENERATED_BODY()
	
	UPROPERTY()
	float TimelineTimestamp = 0.f;

	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, FActorSnapshot> ActorStates; 
};