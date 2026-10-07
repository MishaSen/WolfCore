// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "WolfQACheatManager.generated.h"

class AActor;
class AWolfCharacterBase;
class UAbilitySystemComponent;
class UCombatModeSubsystem;

/**
 * QA instrumentation layer for WolfCore (implementation spec:
 * docs/specs/WolfCoreQACommands_Implementation.md).
 *
 * Every command accelerates reaching a known state or reading a known state for a manual QA test;
 * none of them fake a mechanic's outcome (no forced tag grants, no forced TB-exit reasons, no
 * bypassed clamps). Routed through UCheatManager so console typing works in PIE, and excluded from
 * shipping builds via the #if !UE_BUILD_SHIPPING CheatClass assignment in AWolfPlayerController's
 * constructor (belt-and-suspenders on top of AGameModeBase::AllowCheats()).
 *
 * Every command is dual-marked Exec + BlueprintCallable: Exec for console typing during manual PIE
 * sessions, BlueprintCallable for reflection-based invocation (Remote Control API / MCP bridges).
 *
 * Resource setters route through the real UResourceGainEffect GE pipeline
 * (UCombatModeSubsystem::ApplyResourceGainToPlayer) so PreAttributeChange clamps and
 * PostGameplayEffectExecute logging fire exactly as they would for a real hit — which is what makes
 * tests like "Flow Gauge cannot exceed the configured soft cap" valid.
 */
UCLASS()
class WOLFCORE_API UWolfQACheatManager : public UCheatManager
{
	GENERATED_BODY()

	// ============================================================================================================================
	// Layer 1 — State Injection
	// ============================================================================================================================

public:
	/** Sets the player's FlowGauge to Value through the real UResourceGainEffect gain path — the
	  * same clamps (PreAttributeChange) and logging (PostGameplayEffectExecute) a real hit uses.
	  * Setting above MaxFlowGauge is a valid, useful call: it directly exercises the soft-cap clamp
	  * (test 33). */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_SetFlow(float Value);

	/** Sets the player's Adrenaline to Value through the real UResourceGainEffect gain path (same
	  * GAS pipeline as QA_SetFlow). */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_SetAdrenaline(float Value);

	/** Pokes the resolved target's Health to Value via ApplyModToAttribute(Override). This runs
	  * PreAttributeChange's clamp (0..MaxHealth) but NOT PostGameplayEffectExecute, so setting
	  * Health to 0 does NOT trigger Die() — intentional, for quick baseline pokes. Use
	  * QA_KillTarget for real lethal setup. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_SetHealth(float Value, FString TargetName = TEXT(""));

	/** Kills the resolved target for real: calls AWolfCharacterBase::Die() — the exact same call
	  * PostGameplayEffectExecute makes on a real lethal hit (collision disabled, movement disabled,
	  * InputState.Dead tag, "has died" message). */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_KillTarget(FString TargetName = TEXT(""));

	/** Manual reverse of Die() (production has none): re-enables the capsule (same calls
	  * RestorePhysics uses), re-enables CharacterMovement, clears the InputState.Dead tag (same call
	  * RestoreGAS uses), and restores Health to MaxHealth. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_RestoreTarget(FString TargetName = TEXT(""));

	/** Clears the resolved combatant's AI blackboard TargetActor (the exact key every existing
	  * blackboard read uses). Gives test 1 a deterministic targetless AI. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_ClearTarget(FString TargetName = TEXT(""));

	/** Sets a specific AI's blackboard TargetActor deliberately, rather than relying on
	  * perception/proximity. AIName resolves like any target name (substring over tracked
	  * combatant actor names); empty TargetName resolves to the player pawn. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_SelectTarget(FString AIName, FString TargetName);

	/** Umbrella precondition-reset for nearly every QA test: zeroes the player's Flow and
	  * Adrenaline (via the ApplyResourceGainToPlayer delta pattern), calls QA_RestoreTarget on every
	  * tracked combatant, and if currently in TB, abandons it (SetMode(RT)) so every test starts
	  * from a known RT baseline. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_ResetCombat();

	// ============================================================================================================================
	// Layer 2 — Scenario Construction
	// ============================================================================================================================

	/** Best-effort interrupt scenario: teleports Attacker and Victim to a fixed close spacing,
	  * facing each other, and clears both their blackboard targets so FPresageOrchestrator's stub
	  * target resolution picks each other. Whether the ability windows actually overlap on the baked
	  * timeline is still influenced by intent selection/disposition rolls — this makes overlap
	  * likely, not guaranteed. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_Scenario_Interrupt(FString AttackerName, FString VictimName);

	/** Teleports the named enemy within attack range of the player, facing the player — the setup
	  * test 14 (TB damage-exit) needs for a plan where an attack lands on the player. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_Scenario_TBDamageExit(FString AttackerName);

	/** Same positioning pattern as the interrupt scenario, tuned so both the launch ability and the
	  * follow-up airborne-extension ability can land on the same victim without repositioning
	  * between them. Serves tests 48–51. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_Scenario_AirborneExtension(FString AttackerName, FString VictimName);

	/** Repositions the first Count tracked non-player combatants into a line in front of the player,
	  * Spacing cm apart, facing away from the player — the setup test 52's piercing skillshot needs.
	  * Pure positioning; linking still happens through the real ULinkAbility fire. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_Scenario_LinkedEnemies(int32 Count, float Spacing);

	// ============================================================================================================================
	// Layer 3 — Observability
	// ============================================================================================================================

	/** Single top-to-bottom snapshot: mode, TB phase, timeline time, budgeted TB duration, player
	  * resources/health, then one line per non-player combatant with health and its
	  * RT/TB/Dead/Link/Airborne tag subset. All via WOLF_LOG (Output Log block, not on-screen). */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_DumpCombatState();

	/** Narrower resource cut: player Flow, MaxFlow, computed threshold Stage, Adrenaline, and the
	  * computed spend scalar — giving tests 44–46 an expected value to compare observed results
	  * against. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_DumpResources();

	/** Resolved target's owned gameplay tags, logged as a flat list. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_DumpTags(FString TargetName = TEXT(""));

	/** Resolved target's active GameplayEffect class names + remaining durations. Replaces the
	  * active-GE portion of showdebug abilitysystem. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_DumpEffects(FString TargetName = TEXT(""));

	/** Per tracked combatant, logs its baked planned-intent slice (ability class + start time). */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_DumpPlan();

	/** Iterates the current bake's impact ledger, logging per FPresageImpactEntry: attacker/victim
	  * names, impact time, connection, resource gains, and per-delta attribute/amount/predictability. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_DumpImpactLedger();

	/** Resolved target's time until the next hit impact (GetTimeToNextHitImpact), logged on demand.
	  * Fully resolves test 4 without a Blueprint widget or a permanent per-tick log. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_DumpTimeToNextHitImpact(FString TargetName = TEXT(""));

	// ============================================================================================================================
	// Layer 4 — Assertions
	// ============================================================================================================================

	/** Generic numeric assertion. WOLF_INFO (green, on-screen) on pass, WOLF_ERROR (red) on fail;
	  * both logged plainly so a session transcript has a clean PASS/FAIL trail. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_AssertFloat(FString Label, float Actual, float Expected, float Tolerance = 0.01f);

	/** Resolves TagName via UGameplayTagsManager, checks the resolved target's ASC for it, and
	  * PASS/FAIL-logs against bExpected (covers Status.Link, Status.Airborne, InputState.* checks). */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_AssertTag(FString TargetName, FString TagName, bool bExpected);

	// ============================================================================================================================
	// Layer 5 — TB Control
	// ============================================================================================================================

	/** Locks in the current TB planning session (LockInPlan — already public C++, this wrapper just
	  * makes it console/reflection-reachable). */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_LockInTB();

	/** Precise numeric scrub of the TB timeline to Time seconds — lets tests land on an exact frame
	  * instead of hunting with the analog scrub input. */
	UFUNCTION(Exec, BlueprintCallable, Category = "WolfCore|QA")
	void QA_ScrubTB(float Time);

	// ============================================================================================================================
	// Private Helpers
	// ============================================================================================================================

private:
	/** Shared target resolution (spec's "Target resolution convention"): empty TargetName resolves
	  * to the local player's pawn; non-empty does a case-insensitive substring match against
	  * CMS->GetTrackedCombatants() actor names. First match wins; a WOLF_WARN fires if more than one
	  * tracked combatant matches. Returns nullptr (with a WOLF_WARN) if nothing resolves. */
	AActor* ResolveQATarget(const FString& TargetName) const;

	/** The CMS for this session, or nullptr when unavailable (via UWolfFunctionLibrary). */
	UCombatModeSubsystem* GetCMS() const;

	/** The resolved target's ASC, or nullptr. */
	UAbilitySystemComponent* GetTargetASC(AActor* Target) const;

	/** QA_RestoreTarget's shared implementation — also used by QA_ResetCombat for every tracked
	  * combatant. No-op with a WOLF_WARN if Target isn't a WolfCharacterBase with an ASC. */
	void RestoreCombatant(AActor* Target) const;

	/** QA_ClearTarget's shared implementation — sets the blackboard TargetActor to null. Used by the
	  * interrupt scenario to unqueue both combatants. */
	void ClearCombatantTarget(AActor* Target) const;
};