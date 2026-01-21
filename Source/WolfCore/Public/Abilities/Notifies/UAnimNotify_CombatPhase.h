// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "UAnimNotify_CombatPhase.generated.h"

/**
 * No idea what I made this for. Flag for deletion. 
 */
UCLASS()
class WOLFCORE_API UUAnimNotify_CombatPhase : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Phase")
	FName NotifyName = NAME_None;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	                    const FAnimNotifyEventReference& EventReference) override;
};
