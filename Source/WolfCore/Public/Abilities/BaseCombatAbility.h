// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Presage/PresageOrchestratorTypes.h"
#include "BaseCombatAbility.generated.h"

// ============================================================================================================================
// Enums
// ============================================================================================================================

/** Enumerates the different period types within a combat ability sequence. */
UENUM()
enum class EPeriodType : uint8
{
	/** The windup phase where animation begins but no attack has connected yet. */
	Windup,
	/** The active attack phase where damage and hit detection occur. */
	Attack,
	/** A waiting period between attacks before proceeding to the next sequence. */
	Wait,
	/** An evasion phase where the character dodges incoming attacks. */
	Evasion,
	/** A movement-to-target phase for repositioning during combat sequences. */
	MoveTo
};

/**
 * Describes a single GameplayEffect application tied to a combat period — the effect class,
 * its magnitude, and whether it targets the source (self) or the target actor.
 */
USTRUCT(BlueprintType)
struct FCombatHitEffect
{
	GENERATED_BODY()

	/** GameplayEffect to apply. If unset, this entry is skipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WolfCore|Hit Effects")
	TSubclassOf<UGameplayEffect> EffectClass;

	/** Magnitude passed via SetByCaller. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WolfCore|Hit Effects")
	FScalableFloat Amount = 0.f;

	/** True = apply to the source (attacking) actor. False = apply to the target actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WolfCore|Hit Effects")
	bool bSelfTarget = false;

	/**
	 * SetByCaller tag for the magnitude. Left invalid by default; UBaseCombatAbility::ApplyHitEffects
	 * falls back to FWolfGameplayTags::Get().Data_Amount when this is unset, matching current
	 * behavior where every effect used the same tag. Can be overridden per-entry if a future GE
	 * needs a distinct SetByCaller tag.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WolfCore|Hit Effects")
	FGameplayTag DataAmountTag;
};

/**
 * Describes a single period within a combat ability sequence (montage, duration, attribute effects).
 */
USTRUCT(BlueprintType)
struct FCombatPeriod
{
	GENERATED_BODY()

	/** The type of this combat period (windup, attack, wait, etc.). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WolfCore|Period")
	EPeriodType Type = EPeriodType::Attack;

	/** The animation montage to play for this period's sequence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WolfCore|Period")
	UAnimMontage* Montage = nullptr;

	/** Duration of this period in seconds (fallback value if no montage is assigned). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WolfCore|Period")
	float Duration = 0.5f;

	/** Hit delay time in seconds for Presage prediction when no montage notify is available. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WolfCore|Period")
	float HitDelay = 0.2f;

	// --- Attribute Data ---

	/** All GameplayEffects applied when this period's hit resolves (damage, resource gain, etc). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WolfCore|Attribute Effects")
	TArray<FCombatHitEffect> HitEffects;

	/** Attack range in centimeters for hit detection and collision prediction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WolfCore|Movement")
	float Range = 150.f;

	/** Destination FVector for movement-to targets during MoveTo periods. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Movement")
	FVector MoveToDestination = FVector::ZeroVector;
};

/**
 * Base combat ability with period-based sequence execution and Presage integration.
 */
UCLASS()
class WOLFCORE_API UBaseCombatAbility : public UGameplayAbility
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

public:
	/** Default constructor for UBaseCombatAbility. */
	UBaseCombatAbility();

	/** Overrides UGameplayAbility::ActivateAbility to broadcast OnAbilityActivated after parent execution. */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Overrides UGameplayAbility::EndAbility to broadcast OnAbilityDeactivated before parent execution. */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// ============================================================================================================================
	// Ability Lifecycle Events
	// ============================================================================================================================

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityActivated, UBaseCombatAbility*, Ability);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityDeactivated, UBaseCombatAbility*, Ability);

	UPROPERTY(BlueprintAssignable, Category = "WolfCore|Events")
	FOnAbilityActivated OnAbilityActivated;

	UPROPERTY(BlueprintAssignable, Category = "WolfCore|Events")
	FOnAbilityDeactivated OnAbilityDeactivated;

	// ============================================================================================================================
	// Combat Data Configuration
	// ============================================================================================================================

	/** Returns the complete animation and attack sequence for this ability. */
	const TArray<FCombatPeriod>& GetAbilitySequence() const { return AbilitySequence; }

	/** GameplayTag used to identify input bindings that trigger the startup of this combat sequence. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Combat Data")
	FGameplayTag StartupInputTag;

	/** GameplayTag representing the hit event that triggers attack resolution and damage application. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Combat Data")
	FGameplayTag HitEventTag;

	/** Identifies the "nature" of this ability for reactive planning-phase reads (e.g. avoiding a
	  * redundant repeat against the same target) — not used for gameplay logic, only for the Presage
	  * orchestrator's negotiation phase. Author using an Ability.Archetype.* convention, e.g.
	  * Ability.Archetype.HeavyMelee, Ability.Archetype.Ranged, Ability.Archetype.Defensive. Leave
	  * unset (invalid) if this ability doesn't need to participate in archetype-based comparisons —
	  * an unset tag is never treated as matching another unset tag (see PresageOrchestrator.cpp). */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Combat Data")
	FGameplayTag ArchetypeTag;

	/** Per-ability interrupt response options, used instead of UWolfCombatSettings::
	  * DefaultInterruptResponses when non-empty. See FInterruptResponseOption. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Presage")
	TArray<FInterruptResponseOption> InterruptResponses;

	// ============================================================================================================================
	// Presage API
	// ============================================================================================================================

	/** Calculates the projected time until impact based on current prediction state and animation timing. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Calculate Projected Impact Time"), Category = "WolfCore|Presage")
	/**
	 * @return Float representing the projected time to impact in seconds.
	 */
	float CalculateProjectedImpactTime() const;

	/** Returns the current index within the ability sequence that is actively executing. */
	int32 GetCurrentPeriodIndex() const { return CurrentPeriodIndex; }

	/** Sets the current period index in the ability sequence for direct state manipulation. */
	/**
	 * @param NewIndex The new period index to set as the active sequence position.
	 */
	void SetCurrentPeriodIndex(int32 NewIndex) { CurrentPeriodIndex = NewIndex; }

	/** Checks whether the character is invulnerable at a given time relative to the current prediction window. */
	/**
	 * @param RelativeTime Time delta to query against the prediction buffer.
	 * @return True if the character is invulnerable at the specified time; false otherwise.
	 */
	bool IsInvulnerableAt(float RelativeTime) const;

	/** Returns the duration of a combat period from its configuration data. Static helper method. */
	/**
	 * @param Period The FCombatPeriod to retrieve duration information from.
	 * @return Float representing the period duration in seconds.
	 */
	static float GetPeriodDuration(const FCombatPeriod& Period);

	/** Returns the progress through the current combat period as a normalized value between 0 and 1. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Get Period Progress"), Category = "WolfCore|Presage")
	/**
	 * @return Float representing the progress (in seconds) of the current period (0.0 to 1.0).
	 */
	float GetPeriodProgress() const;

	/** Calculates the duration required for movement between two points given velocity and acceleration parameters. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Calculate Movement Duration"), Category = "WolfCore|Presage")
	/**
	 * @param TotalDistance The total distance to travel in centimeters.
	 * @param MaxVelocity The maximum achievable velocity in cm/s.
	 * @param Acceleration The acceleration rate in cm/s².
	 * @param StartVelocity The initial velocity at the start of movement in cm/s.
	 * @return Float representing the estimated movement duration in seconds.
	 */
	static float CalculateMovementDuration(float TotalDistance, float MaxVelocity, float Acceleration, float StartVelocity);

	/** Initializes a transient ability instance for presage simulation with a copied sequence. */
	void InitializeForSimulation(const TArray<FCombatPeriod>& InSequence);

	/** Resolves MoveTo destinations in a sequence using a source location and target actor. */
	static void ResolveMoveToDestinations(TArray<FCombatPeriod>& Sequence, const FVector& SourceLocation, const AActor* Target);

	/**
	 * Computes a derived timing breakdown (windup / active window / recovery) for the given
	 * ability sequence. The active window is defined as the span from the first period containing
	 * at least one FCombatHitEffect through the last such period. Sequences with no hit-capable
	 * periods are treated as pure windup with no active window and no recovery.
	 * This is a pure function of the sequence data — it does not read any instance state and does
	 * not require a valid ability instance to call.
	 */
	static FAbilityTimingProfile ComputeAbilityTiming(const TArray<FCombatPeriod>& Sequence);

	// ============================================================================================================================
	// Execution State (Protected)
	// ============================================================================================================================

protected:
	/** Array of FCombatPeriod entries defining the complete animation and attack sequence for this ability. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Combat Data")
	TArray<FCombatPeriod> AbilitySequence;

	/** Index of the current period within the ability sequence that is actively executing. */
	int32 CurrentPeriodIndex = 0;

	/** Timestamp marking when the current period began its execution in seconds. */
	float CurrentPeriodStartTime = 0.f;

	// --- Main Loop ---

	/** Initiates the complete combat sequence from the beginning of the ability's AbilitySequence array. */
	UFUNCTION()
	void StartCombatSequence();

	/** Executes a waiting period, pausing the sequence for the specified duration before continuing. */
	/**
	 * @param Period The FCombatPeriod containing wait timing and configuration data.
	 */
	void ExecuteWait(const FCombatPeriod& Period);

	/** Executes an animated period by playing the associated montage and handling animation notifies. */
	/**
	 * @param Period The FCombatPeriod containing montage and attack configuration data.
	 */
	void ExecuteAnimatedPeriod(const FCombatPeriod& Period);

	/** Advances the sequence to the next period in the ability's combat timeline. */
	UFUNCTION()
	void PlayNextPeriod();

	/** Executes a movement-to-target phase, moving the character toward the specified destination. */
	/**
	 * @param Period The FCombatPeriod containing movement parameters and target location.
	 */
	void ExecuteMoveTo(FCombatPeriod& Period);

	/** Retrieves the target actor from the blackboard data for attack targeting and combat resolution. */
	AActor* GetTargetFromBlackboard() const;

	/** Handles completion events for the current period, triggering transitions to the next sequence phase. */
	UFUNCTION()
	void OnPeriodCompleted();

	/** Handles event notifications received during animation montages for hit detection and state changes. */
	/**
	 * @param EventData The FGameplayEventData containing event context and parameters.
	 */
	UFUNCTION()
	void OnEventReceived(FGameplayEventData EventData);

	/** Processes the attack hit event, applying damage, flow gain, and collision prediction for the current period. */
	/**
	 * @param CurrentAttackPeriod Reference to the FCombatPeriod currently executing its attack phase.
	 */
	virtual void HandleAttackHitEvent(const FCombatPeriod& CurrentAttackPeriod);

	// ============================================================================================================================
	// Hit Effect Application
	// ============================================================================================================================

	/**
	 * Applies every FCombatHitEffect entry in Period between this ability's source actor and
	 * TargetActor. Central extension point so derived abilities (RT trace-based, TB
	 * prediction-based) share one application path. Virtual so a derived ability can override
	 * application semantics if it ever needs to (e.g. different mitigation rules) without touching
	 * callers.
	 *
	 * @param Period      The period whose HitEffects should be applied.
	 * @param TargetActor The resolved target actor (RT: trace hit actor; TB: blackboard target).
	 * @param HitResult    Optional trace hit result, added to the effect context if present.
	 * @return True if at least one effect was applied. Kept as bool since RT's debug draw and
	 *         potential future callers can use it; if it ends up unused by every caller, this can
	 *         be simplified to void later.
	 */
	virtual bool ApplyHitEffects(const FCombatPeriod& Period, AActor* TargetActor, const FHitResult* HitResult = nullptr);
};
