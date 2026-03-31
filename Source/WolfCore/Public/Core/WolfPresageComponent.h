// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Presage/ActorSnapshot.h"
#include "Presage/Snapshot.h"
#include "WolfPresageComponent.generated.h"

class AWolfCharacterBase;
class UWolfAbilitySystemComponent;
class UCombatModeSubsystem;
class UCharacterMovementComponent;
class UWolfAbilityComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WOLFCORE_API UWolfPresageComponent : public UActorComponent, public ISnapshot
{
	GENERATED_BODY()

public:
	UWolfPresageComponent();

	virtual void BeginPlay() override;
	void ClearPredictionBuffer(float MaxDuration);
	const FActorSnapshot* GetSnapshotAtTime(float RelativeTime) const;

	void SetIsSimulating(bool bState) { bIsSimulating = bState; }
	void SetSimulationTransform(const FTransform& NewTransform) { SimulationTransform = NewTransform; }
	virtual void SimulateTick(float DeltaTime);

	virtual void CreateSnapshot_Implementation(FActorSnapshot& OutSnapshot) override;
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot) override;

private:
	void SimulatePhysicsStep(float DeltaTime);
	void SnapshotPhysics(FActorSnapshot& Snapshot) const;
	void RestorePhysics(const FActorSnapshot& Snapshot);
	
	void SimulateAnimationStep(float DeltaTime);
	void SnapshotAnim(FActorSnapshot& Snapshot) const;
	void RestoreAnim(const FActorSnapshot& Snapshot);

	void SnapshotGAS(FActorSnapshot& Snapshot) const;
	void RestoreGAS(const FActorSnapshot& Snapshot);

	FORCEINLINE FVector GetOwnerLocation() const;
	FORCEINLINE	FRotator GetOwnerRotation() const;
	FORCEINLINE FVector GetSimLocation() const;
	FORCEINLINE FRotator GetSimRotation() const;
	
	FORCEINLINE UCharacterMovementComponent* GetMoveComp() const;
	FORCEINLINE UWolfAbilitySystemComponent* GetASC() const;
	FORCEINLINE UAnimInstance* GetAnimInst() const;
	FORCEINLINE UAnimMontage* GetCurrentMontage() const;
	FORCEINLINE UCombatModeSubsystem* GetCMS() const;

private:
	bool bIsRestoringSnapshot = false;
	bool bIsSimulating = false;

	FTransform SimulationTransform;
	
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TArray<FActorSnapshot> PredictionBuffer;
	
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TObjectPtr<AWolfCharacterBase> CharacterOwner;
	
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TObjectPtr<UWolfAbilityComponent> AbilityControl;
	
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wolf|Internal")
	TObjectPtr<UWolfAbilitySystemComponent> CachedASC;
};