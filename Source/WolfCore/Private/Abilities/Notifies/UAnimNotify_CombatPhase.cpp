// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/Notifies/UAnimNotify_CombatPhase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/RTCombatAbility.h"
#include "Debug/WolfDebug.h"

void UUAnimNotify_CombatPhase::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                      const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp) return;
	
	const auto* ASC = MeshComp->GetOwner()
		                  ? MeshComp->GetOwner()->FindComponentByClass<UAbilitySystemComponent>()
		                  : nullptr;
	if (!ASC) return;

	for (const auto& Spec : ASC->GetActivatableAbilities())
	{
		if (auto* ActiveAbility = Cast<URTCombatAbility>(Spec.GetPrimaryInstance()))
		{
			ActiveAbility->OnNotifyReceived(NotifyName);
			WOLF_INFO(TEXT("CombatPhase notify sent: %s"), *NotifyName.ToString());
			return;
		}
	}
}
