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
	UWolfAbilitySystemComponent* GetWolfASC() const;

	virtual void SimulateTick(float DeltaTime);

	virtual void CreateSnapshot_Implementation(FActorSnapshot& OutSnapshot) override;
	virtual void RestoreSnapshot_Implementation(const FActorSnapshot& Snapshot) override;

	const FActorSnapshot* GetSnapshotAtTime(float RelativeTime) const;
	UCombatModeSubsystem* GetCMS() const;
	void ClearPredictionBuffer() { PredictionBuffer.Empty(); }

protected:
	void SimulatePhysicsStep(float DeltaTime);
	void SimulateAnimationStep(float DeltaTime);

	void SnapshotPhysics(FActorSnapshot& Snapshot) const;
	void SnapshotGAS(FActorSnapshot& Snapshot) const;
	void SnapshotAnim(FActorSnapshot& Snapshot) const;

	void RestorePhysics(const FActorSnapshot& Snapshot);
	void RestoreGAS(const FActorSnapshot& Snapshot);
	void RestoreAnim(const FActorSnapshot& Snapshot);

	FORCEINLINE UCharacterMovementComponent* GetMoveComp() const { return CachedMoveComp; }
	FORCEINLINE UAnimInstance* GetAnimInst() const { return CachedAnimInst; }
	UAnimMontage* GetWolfCurrentMontage() const { return CachedAnimInst ? CachedAnimInst->GetCurrentActiveMontage() : nullptr; }
	
protected:
	UPROPERTY(BlueprintReadOnly, Category = "Wolf|Cache")
	TObjectPtr<UCharacterMovementComponent> CachedMoveComp;

	UPROPERTY(BlueprintReadOnly, Category = "Wolf|Cache")
	TObjectPtr<UAnimInstance> CachedAnimInst;

	UPROPERTY(BlueprintReadOnly, Category = "Wolf|Cache")
	mutable TWeakObjectPtr<UCombatModeSubsystem> CachedCMS;

	UPROPERTY()
	TObjectPtr<UWolfAbilitySystemComponent> CachedASC;

private:
	UPROPERTY()
	TArray<FActorSnapshot> PredictionBuffer;

	UPROPERTY()
	bool bIsRestoringSnapshot = false;

	UPROPERTY()
	TObjectPtr<AWolfCharacterBase> CharacterOwner;

	UPROPERTY()
	TObjectPtr<UWolfAbilityComponent> AbilityControl;

	FORCEINLINE FVector GetOwnerLocation() const { return CharacterOwner ? CharacterOwner->GetActorLocation() : FVector::ZeroVector; }
	FORCEINLINE FRotator GetOwnerRotation() const { return CharacterOwner ? CharacterOwner->GetActorRotation() : FRotator::ZeroRotator;}
};