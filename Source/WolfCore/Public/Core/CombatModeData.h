#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "CombatModeData.generated.h"

class UInputMappingContext;
enum class EInputContext : uint8;
enum class ECombatMode : uint8;

USTRUCT(BlueprintType)
struct FCombatModeInfo : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag Tag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UInputMappingContext> InputMapping;
};