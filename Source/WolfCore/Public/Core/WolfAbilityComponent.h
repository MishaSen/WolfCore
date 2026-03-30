// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/WolfAbilitySystemComponent.h"
#include "Components/ActorComponent.h"
#include "WolfAbilityComponent.generated.h"


class UCharacterStatConfig;
class UWolfAttributeSet;
class UWolfAbilitySystemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WOLFCORE_API UWolfAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWolfAbilityComponent();

	UFUNCTION(BlueprintCallable, Category = "Wolf|Abilities")
	UWolfAbilitySystemComponent* GetWolfASC() const { return CacheASC; }

	void InitializeAbilitySystem(AActor* InOwner);
	void ApplyDefaultAttributes();
	void AddStartupAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities);

	FORCEINLINE const TArray<FGameplayAttribute>& GetCachedAttributes() const { return CachedAttributes; }
	FORCEINLINE const UCharacterStatConfig* GetStatConfig() const { return StatConfig; }

protected:
	UPROPERTY()
	TObjectPtr<UWolfAbilitySystemComponent> CacheASC;

	UPROPERTY()
	TObjectPtr<UWolfAttributeSet> AttributeSet;

	UPROPERTY(EditAnywhere, Category = "Wolf|Abilities")
	TObjectPtr<UCharacterStatConfig> StatConfig;

	UPROPERTY(EditAnywhere, Category = "Wolf|Abilities")
	TSubclassOf<UGameplayEffect> DefaultAttributes;

private:
	UPROPERTY()
	TArray<FGameplayAttribute> CachedAttributes;
};
