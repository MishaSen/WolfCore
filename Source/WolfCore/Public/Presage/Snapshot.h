// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Snapshot.generated.h"

struct FActorSnapshot;

UINTERFACE(MinimalAPI)
class USnapshot : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WOLFCORE_API ISnapshot
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Snapshot")
	virtual void CreateSnapshot(FActorSnapshot& OutSnapshot);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Snapshot")
	virtual void RestoreSnapshot(const FActorSnapshot& InSnapshot);
};