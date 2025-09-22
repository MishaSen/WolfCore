// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/Notifies/AnimNotify_Hit.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/TBCombatAbility.h"

void UAnimNotify_Hit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                             const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner()))
	{
		if (GameplayTag.IsValid())
		{
			FGameplayEventData Payload;
			Payload.EventTag = GameplayTag;
			ASC->HandleGameplayEvent(GameplayTag, &Payload);
		}
	}
}
