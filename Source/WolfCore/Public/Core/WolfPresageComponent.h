// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/WolfCharacterBase.h"
#include "Components/ActorComponent.h"
#include "Presage/ActorSnapshot.h"
#include "Presage/Snapshot.h"
#include "WolfPresageComponent.generated.h"

class UWolfAbilitySystemComponent;
class UCombatModeSubsystem;
class UCharacterMovementComponent;
class UWolfAbilityComponent;
class AWolfCharacterBase;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WOLFCORE_API UWolfPresageComponent : public UActorComponent, public ISnapshot
{
	GENERATED_BODY()

public:
	UWolfPresageComponent();

	virtual void BeginPlay() override;

	virtual void SimulateTick(float DeltaTime);
	void ClearPredictionBuffer(float MaxDuration);
	const FActorSnapshot* GetSnapshotAtTime(float RelativeTime) const;

	virtual void CreateSnapshot_Implementation(FActorSnapshot& OutSnapshot) override;
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot) override;

public:
	bool bIsSimulating = false;
	
private:
	void SimulatePhysicsStep(float DeltaTime);
	void SimulateAnimationStep(float DeltaTime);

	void SnapshotPhysics(FActorSnapshot& Snapshot) const;
	void SnapshotGAS(FActorSnapshot& Snapshot) const;
	void SnapshotAnim(FActorSnapshot& Snapshot) const;

	void RestorePhysics(const FActorSnapshot& Snapshot);
	void RestoreGAS(const FActorSnapshot& Snapshot);
	void RestoreAnim(const FActorSnapshot& Snapshot);

	FORCEINLINE FVector GetOwnerLocation() const { return CharacterOwner ? CharacterOwner->GetActorLocation() : FVector::ZeroVector; }
	FORCEINLINE	FRotator GetOwnerRotation() const { return CharacterOwner ? CharacterOwner->GetActorRotation() : FRotator::ZeroRotator; }
	FORCEINLINE FVector GetSimLocation() const { return bIsSimulating ? SimulationTransform.GetLocation() : GetOwnerLocation(); }
	FORCEINLINE FRotator GetSimRotation() const { return bIsSimulating ? SimulationTransform.GetRotation() : GetOwnerRotation(); }
	
	FORCEINLINE UCharacterMovementComponent* GetMoveComp() const { return CharacterOwner->GetCharacterMovement(); }
	FORCEINLINE UWolfAbilitySystemComponent* GetASC() const { return CachedASC; }
	FORCEINLINE UAnimInstance* GetAnimInst() const { return CharacterOwner->GetAnimInst(); }
	FORCEINLINE UAnimMontage* GetCurrentMontage() const { return GetAnimInst() ? GetAnimInst()->GetCurrentActiveMontage() : nullptr; }
	FORCEINLINE UCombatModeSubsystem* GetCMS() const { return CharacterOwner ? CharacterOwner->GetCMS() : nullptr; }

private:
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TObjectPtr<AWolfCharacterBase> CharacterOwner;

	FTransform SimulationTransform;
	
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TObjectPtr<UWolfAbilityComponent> AbilityControl;
	
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TObjectPtr<UWolfAbilitySystemComponent> CachedASC;
	
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TArray<FActorSnapshot> PredictionBuffer;
	
	bool bIsRestoringSnapshot = false;
};