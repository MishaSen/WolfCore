// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/WolfGameplayTags.h"
#include "Math/RandomStream.h"
#include "Presage/ActorSnapshot.h"
#include "Presage/PresageImpactLedger.h"
#include "Presage/PresageOrchestratorTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/IWolfCombatant.h"
#include "Tickable.h"
#include "CombatModeSubsystem.generated.h"

struct FStreamableHandle;
class UAbilitySystemComponent;
class UWolfCombatant;
class UBaseCombatAbility;

/**
 * Delegate broadcast when the global combat mode changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatModeChanged, FGameplayTag, NewMode);

/**
 * THE EXACT-PREVIEW CONTRACT (normative — binding on PresagePreview stages 2–3 and all future
 * TB-adjacent work; see PresagePreviewStage1.md for full rationale).
 *
 * In-contract (must round-trip on scrub, must match between preview and execution):
 *   - Actor transform (location, rotation) and velocity.
 *   - Active montage identity and playback position.
 *   - Active ability identity and current period index.
 *   - All attributes registered in UCharacterStatConfig (Health, MaxHealth, FlowGauge,
 *     Adrenaline) — once stage 2 makes them change during the bake.
 *   - Predicted death (health reaching 0 in the bake) — as data/presentation only during
 *     preview; see below.
 *
 * Out-of-contract (consciously excluded):
 *   - GameplayEffect object instances. Preview never applies real GEs (stage 3 enforces this).
 *     A restore-by-reapplication path can never be provably exact (durations, periodic-tick
 *     phase, and stack state don't survive reconstruction) — removing real effects from preview
 *     makes the contract trivially satisfiable instead of heroically maintained.
 *   - Cooldowns. No ability meaningfully uses them yet; excluded until one does.
 *   - AI blackboard targets. Resolved into FIntentEntry.Target at plan time; the blackboard is a
 *     planning INPUT, not previewed state.
 *
 * Named intentional behaviors:
 *   - RestoreGAS's dead-tag reset is intentional: death is previewable and rewindable. Scrubbing
 *     behind a predicted death shows the actor alive. Nobody "fixes" this later. Real death (with
 *     Die()'s collision/movement side effects) occurs only during execution, via real GE
 *     application (stage 3).
 *   - Predicted death during preview is data + presentation only: health 0 in the snapshot, no
 *     Die() side effects — falls out naturally from stage 2 writing attributes via
 *     SetNumericAttributeBase, which does not fire PostGameplayEffectExecute.
 *
 * MAGNITUDE-PREDICTABILITY RULE (binding on all future ability/effect authoring): every
 * FCombatHitEffect used by a TB-previewable ability must have a numerically predictable
 * magnitude — computable at bake time from the SetByCaller amount at ability level, without
 * live-ASC-dependent inputs. Stage 2 computes predicted deltas from exactly this path; stage 3
 * applies the real GE from exactly this path. A GE whose real application could diverge from its
 * predicted delta is a contract violation. Stage 3's divergence guard should never fire; when it
 * does, it names the exact authored effect that broke this rule.
 */
UENUM(BlueprintType)
enum class ETBPhase : uint8
{
	/** Not in TB at all. */
	None,
	/** TB entered, scene frozen (global dilation 0), whole future baked, player scrubs/selects.
	  * Untimed — the Flow budget maps to execution timeline length, not think-time. */
	Planning,
	/** Lock-in has occurred; the baked plan is playing forward in real (unscaled) time. Still TB
	  * mode, not RT. Cannot be exited before the plan completes except by the damage hard-exit. */
	Executing
};

/** Maps to vision's three TB endings. See UCombatModeSubsystem::ExitTB. */
UENUM(BlueprintType)
enum class ETBExitReason : uint8
{
	/** Reached the end of the budgeted timeline without an ending action — the neutral
	  * "flow depleted / didn't find the play" outcome. */
	PlanCompleted,
	/** An ability tagged as an ending action (UWolfCombatSettings::TBEndingActionArchetypeTag)
	  * executed. Bonus effects are explicitly TBD per vision — this stage recognizes and logs
	  * only; no bonus is applied. */
	EndingAction,
	/** The player or a linked ally took damage during Executing (this ending is only possible
	  * during Executing — nothing real happens during Planning, so nothing can deal damage then). */
	DamageTaken
};

/**
 * World subsystem managing combat mode state (Real-Time / Turn-Based).
 * Handles mode transitions, time dilation, temporal snapshots, and presage simulation coordination.
 */
UCLASS()
class WOLFCORE_API UCombatModeSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	// ============================================================================================================================
	// FTickableGameObject
	// ============================================================================================================================

public:
	/** Advances ExecutionClock by unscaled real delta time and scrubs the timeline to it. No-op
	  * (via IsTickable) outside TBPhase::Executing. */
	virtual void Tick(float DeltaTime) override;

	/** Only ticks while actively playing back a locked-in TB plan. */
	virtual bool IsTickable() const override { return TBPhase == ETBPhase::Executing; }

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UCombatModeSubsystem, STATGROUP_Tickables);
	}

	/** Restricts ticking to this subsystem's own world — without this override,
	  * FTickableGameObject ticks across every world (e.g. every PIE instance), not just this one. */
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

	// ============================================================================================================================
	// Lifecycle
	// ============================================================================================================================

	/** Initializes the subsystem when it is registered with the world, setting up internal state and tag references. */
	/**
	 * @param Collection Reference to the FSubsystemCollectionBase containing all initialized subsystems.
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Called when the world begins play, initializing combat mode state and loading default configuration. */
	/**
	 * @param InWorld Pointer to the UWorld being loaded for subsystem initialization.
	 */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ============================================================================================================================
	// Public API - Mode Management
	// ============================================================================================================================

public:
	/** Sets the global combat mode to a new value, triggering mode transitions and updating all registered combatants. */
	/**
	 * @param NewMode The FGameplayTag representing the new combat mode (e.g., RT, TB).
	 */
	void SetMode(FGameplayTag NewMode);

	/** Switches between combat modes using the player's current selection from the game instance configuration. */
	void SwitchCombatMode();

	/** Applies a specific combat mode to an individual actor, updating their ability system and state accordingly. */
	/**
	 * @param Combatant Pointer to the AActor whose combat mode is being updated (can be any actor type).
	 * @param NewMode The FGameplayTag representing the new combat mode to apply.
	 */
	void ApplyModeToActor(const TScriptInterface<IWolfCombatant>& Combatant, FGameplayTag NewMode);

	/** Returns the current global combat mode as a GameplayTag for Blueprint queries and runtime checks. */
	UFUNCTION(BlueprintPure, Meta = (DisplayName = "Get Current Mode"), Category = "WolfCore|Combat")
	FGameplayTag GetCurrentMode() const { return CurrentMode; }

	/** Returns the current timeline position in seconds representing elapsed time within the active combat mode. */
	UFUNCTION(BlueprintPure, Meta = (DisplayName = "Get Timeline Time"), Category = "WolfCore|Combat")
	float GetCurrentTimelineTime() const { return CurrentTimelineTime; }

	/** Boolean flag indicating whether the current combat mode is set to turn-based (TB) rather than real-time (RT). */
	bool bIsInTB;

	/** Current TB phase. None outside TB; must be Planning or Executing whenever bIsInTB is true. */
	UFUNCTION(BlueprintPure, Category = "WolfCore|Combat")
	ETBPhase GetTBPhase() const { return TBPhase; }

	/** Transitions Planning -> Executing: calls the Flow deduction hook (OnLockInFlowDeduction),
	  * scrubs to 0, and starts execution playback. No-op with a warning log if not currently in
	  * TBPhase::Planning. The "every free linked ally has an action queued" gate is UI-layer
	  * validation — this function does not re-check it; there is no ally roster to check yet. */
	void LockInPlan();

	/** Transitions Executing (or, degenerate case, Planning via manual abandon) back to RT.
	  * Reason determines exit-specific handling (hard-exit debuff, ending-action log). Victims is
	  * only meaningful for ETBExitReason::DamageTaken — each valid entry receives
	  * UWolfCombatSettings::TBHardExitDebuffClass if configured. */
	void ExitTB(ETBExitReason Reason, const TArray<TWeakObjectPtr<AActor>>& Victims = {});

	// ============================================================================================================================
	// Public API - Snapshots & Timeline
	// ============================================================================================================================

	/** Captures the current world state into a FTemporalStates container at the specified timestamp for timeline scrubbing. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Capture World State"), Category = "WolfCore|Combat|Snapshots")
	/**
	 * @param Timestamp The time anchor in seconds at which to capture the world state snapshot.
	 * @return FTemporalStates containing the captured world state mapped to the specified timestamp.
	 */
	FTemporalStates CaptureCurrentWorldState(float Timestamp);

	/** Advances or rewinds the prediction timeline to a specific timestamp for scrubbing and visualization. */
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Scrub Timeline"), Category = "WolfCore|Combat")
	/**
	 * @param NewTime The target timeline position in seconds to scrub the prediction state to.
	 */
	void ScrubTimeline(float NewTime);

	/** Re-bakes the prediction timeline from the master snapshot, preserving the current scrub position. */
	void ReBakeTimeline();

	/** Returns a reference to the master snapshot captured at the start of the current simulation session. */
	const FTemporalStates& GetMasterSnapshot() const { return MasterStartSnapshot; }

	// ============================================================================================================================
	// Public API - Combatant Tracking
	// ============================================================================================================================

	/** Registers a combatant as an active participant, enabling it to receive mode updates and prediction data. */
	/**
	 * @param Combatant TScriptInterface<IWolfCombatant> representing the combatant being registered.
	 */
	void RegisterCombatant(const TScriptInterface<IWolfCombatant>& Combatant);

	/** Unregisters a combatant from active participation, removing it from mode updates and prediction processing. */
	/**
	 * @param Combatant TScriptInterface<IWolfCombatant> representing the combatant being removed.
	 */
	void UnregisterCombatant(const TScriptInterface<IWolfCombatant>& Combatant);

	/** Returns the array of weak references to tracked combatants currently registered with this subsystem. */
	const TArray<TScriptInterface<IWolfCombatant>>& GetTrackedCombatants() const { return TrackedCombatants; }

	/** Returns the fixed step size (seconds) used to produce the current PredictionBuffer contents.
	  * This value is set once at bake-start and read by every UWolfPresageComponent::GetSnapshotAtTime()
	  * call, ensuring index math uses the actual step size rather than a compile-time constant. */
	float GetBakedStepSize() const { return BakedStepSize; }

	/**
	 * Returns the cached timing profile for the given ability class, computing and caching it on
	 * first request via UBaseCombatAbility::ComputeAbilityTiming against the class's CDO sequence.
	 * Returns a default-constructed (all-zero) FAbilityTimingProfile if AbilityClass is null.
	 * Not called from anywhere yet in this stage.
	 */
	const FAbilityTimingProfile& GetOrComputeTimingProfile(const TSubclassOf<UBaseCombatAbility>& AbilityClass);

	/**
	 * Returns the presage planning RNG stream for this session. Re-initialized to
	 * PresageSessionSeed at the start of every planning pass (SetMode's TB branch and every
	 * ReBakeTimeline) — see PresageDeterminism.md. FRandomStream's roll methods (FRand,
	 * RandRange, FRandRange) are const, so a const reference is sufficient for every caller that
	 * only rolls; only SetMode/ReBakeTimeline call Initialize() on the non-const member directly.
	 */
	const FRandomStream& GetPresageRandomStream() const { return PresageRandomStream; }

	// ============================================================================================================================
	// Public API - Impact Ledger (PresagePreview stage 2)
	// ============================================================================================================================

	/** The current bake's impact ledger, time-ordered. Cleared at the start of every bake
	  * (SetMode's TB branch and ReBakeTimeline, immediately before ExecuteFutureBake). Consumed by
	  * CheckExecutionDamageExit and, from PresagePreviewStage3 onward, by real ledger-impact
	  * application during execution playback. */
	const TArray<FPresageImpactEntry>& GetBakeImpactLedger() const { return BakeImpactLedger; }

	/** Appends one resolved impact to the current bake's ledger. Called by
	  * UWolfPresageComponent::ResolveSimulatedImpact during SimulateTick — the sim component
	  * resolves the hit (connection, deltas), the subsystem just owns the ledger storage. */
	void AppendImpactLedgerEntry(const FPresageImpactEntry& Entry) { BakeImpactLedger.Add(Entry); }

	// ============================================================================================================================
	// Events
	// ============================================================================================================================

	/** Multicast delegate broadcast whenever the global combat mode changes, notifying all registered listeners. */
	UPROPERTY(BlueprintAssignable, Category = "WolfCore|Combat")
	FOnCombatModeChanged OnCombatModeChanged;

protected:
	/** Temporal states container holding the master snapshot captured at simulation start for state restoration. */
	FTemporalStates MasterStartSnapshot;

	/** Current timeline position in seconds representing elapsed time within the active combat mode session. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Timeline")
	float CurrentTimelineTime = 0.f;

	/** Maximum allowed duration in seconds for the prediction timeline before requiring regeneration or reset. */
	UPROPERTY(EditDefaultsOnly, Category = "WolfCore|Timeline")
	float MaxTimelineDuration = 5.f;

	/** None outside TB; Planning immediately on TB entry; Executing once LockInPlan() runs. */
	UPROPERTY(BlueprintReadOnly, Category = "WolfCore|Timeline")
	ETBPhase TBPhase = ETBPhase::None;

	/** Elapsed unscaled real time since LockInPlan() started execution playback. Reset to 0 by
	  * LockInPlan(); advanced by Tick(); read by ScrubTimeline, CheckExecutionDamageExit, and
	  * CheckEndingAction every Executing tick. */
	float ExecutionClock = 0.f;

	/** Flow deduction hook, called once by LockInPlan() at lock-in. Empty this stage —
	  * ResourceLoop stage 3 implements the actual deduction (amount = function of threshold stage
	  * consumed; numbers TBD there). This is the one blessed place for that stage to land its
	  * logic. Do not call SetMode or otherwise mutate TB phase state from an override of this. */
	virtual void OnLockInFlowDeduction() {}

	/** Damage hard-exit hook, checked every Executing tick after ScrubTimeline. This stage: stub,
	  * always returns false and leaves OutVictims untouched — there is no impact data to check
	  * against until PresagePreview stage 2 exists. Stage 2 fills the body: scans its impact
	  * ledger for the first not-yet-consumed entry with bConnected && bVictimIsPlayerOrLinked &&
	  * ImpactTime <= InExecutionClock, appends its victim(s) to OutVictims, and returns true. */
	bool CheckExecutionDamageExit(float InExecutionClock, TArray<TWeakObjectPtr<AActor>>& OutVictims);

	/** Ending-action recognition, checked every Executing tick after CheckExecutionDamageExit.
	  * Real this stage: true iff the player's currently-executing baked ability (per the buffer
	  * frame at InExecutionClock) has an ArchetypeTag matching
	  * UWolfCombatSettings::TBEndingActionArchetypeTag. The BONUS an ending action grants is
	  * explicitly TBD per vision ("exact shape TBD") and is not implemented here — ExitTB's
	  * EndingAction branch only logs recognition; nothing else fires. */
	bool CheckEndingAction(float InExecutionClock) const;

	/** Current bake's impact ledger. See GetBakeImpactLedger/AppendImpactLedgerEntry above.
	  * Cleared (and DamageExitCursor reset to 0) at the start of every bake. */
	UPROPERTY(Transient)
	TArray<FPresageImpactEntry> BakeImpactLedger;

	/** Index into BakeImpactLedger of the first not-yet-consumed entry, read by
	  * CheckExecutionDamageExit. The ledger is time-ordered, so a monotonic cursor is sufficient —
	  * no rescanning from zero every tick. Reset to 0 whenever BakeImpactLedger is cleared, and
	  * again by LockInPlan() so a fresh execution playback always starts scanning from the top. */
	int32 DamageExitCursor = 0;

	private:
	/** The fixed step size (seconds) used to produce the current PredictionBuffer contents across
	  * every combatant in the active bake. Set once at the start of ExecuteFutureBake() from
	  * UWolfCombatSettings::PresageSimulationStep; every UWolfPresageComponent::GetSnapshotAtTime()
	  * call reads this via GetBakedStepSize(), so a designer-tuned change to the simulation rate
	  * only needs to update this one path. */
	float BakedStepSize = 0.1f; // Fallback only; overwritten from UWolfCombatSettings in SetMode().

	/** Cache of derived ability timing profiles, keyed by ability class. AbilitySequence is
	  * authored per-class and doesn't vary per instance, so this is computed once per class on
	  * first request rather than recomputed for every planning decision. Not populated or read
	  * anywhere yet — added in this stage as inert storage; stage 3 (planning) is the first
	  * consumer. */
	UPROPERTY()
	TMap<TSubclassOf<UBaseCombatAbility>, FAbilityTimingProfile> TimingProfileCache;

	/** Handles completion of presage effect loading, applying the loaded gameplay effect to the player's ability system. */
	void OnPresageEffectLoaded();

	/** Initializes subsystem defaults by setting up internal state, tag references, and configuration values at startup. */
	void InitializeSubsystemDefaults();

	/** Attempts to load the player-selected combat mode from game instance configuration for initial mode setup. */
	/**
	 * @return True if a valid player-selected mode was loaded; false otherwise (default level mode will be applied).
	 */
	bool TryLoadPlayerSelectedMode();

	/** Applies the default combat mode from level settings when no player selection is available or valid. */
	void ApplyDefaultLevelMode();

	/** Handles presage drain effect application to an actor's ability system based on their current combat mode state. */
	/**
	 * @param ASC Pointer to the UAbilitySystemComponent receiving the drain effect modification.
	 * @param CurrentActorMode The FGameplayTag representing the actor's current combat mode being evaluated.
	 */
	void HandlePresageDrainEffect(UAbilitySystemComponent* ASC, FGameplayTag CurrentActorMode);

	/** Retrieves the player controller's Ability System Component (ASC) for gameplay effect application and state management. */
	UAbilitySystemComponent* GetPlayerASC() const;

	/** Returns the time dilation multiplier configured for a specific combat mode to control temporal prediction scaling. */
	/**
	 * @param Mode The FGameplayTag representing the combat mode whose dilation value is being queried.
	 * @return Float representing the time dilation factor (e.g., 1.0 for RT, 0.5 for TB).
	 */
	static float GetDilationForMode(const FGameplayTag& Mode);

	/** GameplayTag representing the current global combat mode (Real-Time or Turn-Based) active in this session. */
	FGameplayTag CurrentMode;

	/** Singleton gameplay tags structure providing framework-wide tag access for combat mode identification and filtering. */
	FWolfGameplayTags WolfTag;

	/** Shared pointer to a streamable handle managing asynchronous loading of the presage effect class asset. */
	TSharedPtr<FStreamableHandle> PresageClassLoadHandle;

	/** Subclass of GameplayEffect applied during presage simulation for drain effects and temporal state management. */
	UPROPERTY()
	TSubclassOf<UGameplayEffect> PresageEffectClass;

	/** Array of weak script interfaces to combatants currently tracked by this subsystem for mode updates and prediction processing. */
	UPROPERTY()
	TArray<TScriptInterface<IWolfCombatant>> TrackedCombatants;

	/** Seeded once per TB entry (SetMode's TB branch, before the first RunPlanning) via
	  * FMath::Rand(), logged so a bad plan is reproducible from the log. Re-used (not
	  * re-randomized) by every ReBakeTimeline within the same TB session — PresageRandomStream is
	  * re-initialized to this same value at the start of every planning pass, which is what makes
	  * "same inputs -> same plan" hold across re-bakes. See PresageDeterminism.md. */
	int32 PresageSessionSeed = 0;

	/** Planning-phase RNG stream, re-initialized to PresageSessionSeed at the start of every
	  * planning pass. Never persists rolls across passes — a stream that merely persisted would
	  * still diverge on the second pass because it would have consumed values from the first. */
	FRandomStream PresageRandomStream;
};