#pragma once

#include "CombatMode.generated.h"

UENUM(BlueprintType)
enum class ECombatMode : uint8
{
	OOC UMETA(DisplayName="Out of Combat"),
	RT UMETA(DisplayName="Real-Time"),
	TB UMETA(DisplayName="Turn-Based")
};
