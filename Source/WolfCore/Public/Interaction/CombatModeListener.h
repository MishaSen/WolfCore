// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatModeListener.generated.h"

enum class ECombatMode : uint8;

UINTERFACE()
class UCombatModeListener : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WOLFCORE_API ICombatModeListener
{
	GENERATED_BODY()

public:
};
