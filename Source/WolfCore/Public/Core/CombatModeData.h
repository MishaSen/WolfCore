#pragma once

#include "CoreMinimal.h"
#include "CombatMode.h"
#include "WolfPlayerController.h"
#include "Engine/DataTable.h"
#include "CombatModeData.generated.h"

enum class EInputContext : uint8;
enum class ECombatMode : uint8;

USTRUCT(BlueprintType)
struct FCombatModeInfo : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ECombatMode Mode = ECombatMode::OOC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EInputContext InputContext = EInputContext::OutOfCombat;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UInputMappingContext> InputMapping;
};