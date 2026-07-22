# PresageOrchestratorArchitecture.md

## Presage: AI Orchestrator & Timeline Decision Model

**Status:** Draft — architectural direction, not an implementation spec
**Scope:** Extends the existing Presage bake/scrub system (`CombatModeSubsystem`, `FWolfPresageSimulator`, `WolfPresageComponent`) with a decision-making layer for AI combatants during turn-based (TB) mode.

---

## 1. Goals

When the player enters TB, the game should not just freeze and let the player act — it should present a **fully-formed predicted timeline**, where every AI combatant has already decided what it's going to do, informed by what everyone else on the timeline is doing. The player then scrubs, watches, and can override their own character's choices, triggering a re-prediction.

Concretely:

- AI ability selection must account for **state dependencies** (a combatant in hitstun cannot act; an ability's own preconditions must hold).
- AI ability selection should account for **cross-actor intent** (an ally is more likely to choose a follow-up ability if it knows another combatant just started a particular attack).
- Decision-making must be **synchronous and effectively instantaneous** — no visible delay when TB is entered.
- The result must be a **scrubbable, presentational timeline** — no re-computation while the player is just scrubbing or watching.
- A **player-initiated change** at any point on the timeline invalidates everything causally downstream of it and triggers a full, deterministic re-run.
- Whether TB starts paused or auto-playing through the timeline is a **UI/UX decision, not an architectural one**, and can be deferred or made toggleable without affecting anything below.

---

## 2. Relationship to the Existing Presage System

This does not replace anything already built — it fills in the one piece that's currently missing.

| Already correct, unchanged | What this doc adds |
|---|---|
| `ExecuteFutureBake`'s fixed-step, synchronous loop | Becomes the **execution** phase — walks each combatant through a plan instead of a single static sequence |
| `ScrubTimeline` reading `PredictionBuffer` — read-only, no re-simulation | Nothing — scrub semantics don't change |
| `ReBakeTimeline` — restore to `MasterStartSnapshot`, re-run the whole bake | Becomes the mechanism that re-runs **planning** (not just execution) whenever the player changes something |
| The bake mutating real actors' transforms/animation in-place during simulation (not shadow copies) | Still applies to execution; planning itself doesn't need this — it reasons abstractly, before physics ever runs |

The player-ability-injection work already in progress (`TryInjectPresageAbility`, `PresageAbilityRequest`, `TryActivateInjectedAbility`) is a **rough first draft of the same underlying mechanism**, scoped down to one actor and one player-driven request. Once its known issues are fixed (request never cleared; a still-active real ability permanently blocking the injected one — see prior review), it becomes the special case of this architecture where the "decision" is supplied by the player instead of the orchestrator.

---

## 3. The Orchestrator: Planning Before Execution

The examples you gave — asking a reactive character "what do you want to do," moving on, then coming back with "here's what happened, decide" — describe something more specific than a same-tick resolution order. It's a **separate planning phase that runs to completion before any physical execution happens**, producing a fully-resolved plan that the existing tick loop then just carries out mechanically. This is why "waiting" never shows up on the timeline: by the time anything gets baked into `PredictionBuffer`, every decision — including every reactive follow-up and every interrupt — has already been settled. The timeline only ever shows finished, committed actions.

Two phases, not one:

- **Planning** — an abstract, lightweight negotiation over intents, entirely decoupled from per-tick physics/animation. Produces a finalized **Action Plan**: for each combatant, an ordered list of `{Ability, Target, StartTime}`.
- **Execution** — the existing fixed-step `SimulateTick` loop, unchanged in mechanism, now walking each combatant through its finalized plan instead of a single static pre-authored sequence — exactly the way `TryActivateInjectedAbility` already hands one ability to that same machinery today, just generalized to a full plan instead of one entry.

Planning has to be lightweight specifically because it *isn't* running physics — it reasons about abilities using their metadata (windup, active-window duration, speed) rather than simulating them tick by tick, which is what makes it plausible for the whole thing to feel instantaneous even though real decision-making is happening.

### 3.1 Intent negotiation

Round-based, matching what you described:

1. **Round 1 — ask everyone.** Every combatant is asked for an intent. A combatant's disposition determines how it answers: it can either commit to something concrete (`Ability X, Target Y, roughly now`) or defer (`watching <Actor>, will decide once I see what they do`).
2. **Round 2 — resolve deferrals.** Anyone who deferred in round 1 is now told what the combatant(s) they were watching committed to, and finalizes their own intent.

This is the same mechanism whether you describe it as a conversation ("what do you want to do?" / "here's what happened, decide") or as tag-grouped passes (ask all `Initiator`-tagged combatants, then all `Reactive`-tagged ones) — the tags are just what determines who answers in round 1 versus who defers to round 2, not a separate design. Framing it as personality/disposition rather than a rigid role partition is the more accurate model: whether a given combatant commits immediately or watches-and-waits should be a **probability-weighted roll per decision**, not a fixed category — so a "reactive" character can still occasionally initiate, and the same tags/stats that express personality just bias that roll rather than gating it absolutely. Order among round-1 committers doesn't matter, matching your point — nobody's plan is final yet at that stage, so who gets asked first has no effect on anything.

### 3.2 Temporal resolution and interrupts

Once every combatant has a committed intent (after however many rounds resolve all deferrals), the orchestrator computes timing using each ability's metadata — windup, active window, recovery, any speed modifiers — to figure out what actually happens if everyone's declared plan played out simultaneously: who lands a hit on whom, and in what order. This is where an interrupt gets detected: if character A's incoming hit would land during character B's own windup, before B's ability resolves, B's plan can't just proceed as declared.

When that happens, the interrupted combatant gets a follow-up ask: *your ability is being interrupted — what's your response?* The available responses (cancel/feint into another ability, dodge, parry, take the hit) should be a **data-driven, weighted table**, not a fixed enum baked into orchestrator logic — something like:

```cpp
struct FInterruptResponseOption
{
    FGameplayTag ResponseTag;   // e.g. Presage.Interrupt.Feint, .Dodge, .Parry, .TakeHit
    float Weight;               // probability weighting
    FGameplayTagContainer RequiredTags; // e.g. only available if this ability has a feint-cancel window
};
```

This means you don't need dodge/parry/feint all implemented on day one — the infrastructure (a per-ability or per-character table of weighted, tag-gated options, resolved by the orchestrator whenever an interrupt is detected) is the actual architectural piece; specific response types are content, added incrementally without touching the orchestrator itself.

One real complexity worth flagging rather than hand-waving: a chosen response can itself be interrupted (a dodge timed badly could still get hit by a third combatant), so this resolution is naturally **iterative**, not a single pass. It needs either a hard iteration cap or a well-defined resolution order to guarantee it actually terminates rather than looping if two combatants' responses keep invalidating each other.

### 3.3 Constraints as gates, not special-cased logic

"Can't act while in hitstun" and similar constraints should be expressed as **ability activation requirements** the orchestrator checks the same way real-time ability activation already checks tag-based blocking/required tags — not as bespoke orchestrator logic. This keeps one source of truth for "what makes an ability valid right now" instead of two (real-time activation rules, and a separate orchestrator rulebook that has to be kept in sync with them by hand — the exact failure mode the sim/real divergence discussion flagged earlier in this project).

---

## 4. The Planning Data Model: An Intent Ledger

Given planning needs to stay lightweight and decoupled from physics, "world state so far" shouldn't be per-frame simulated transforms — it should be an append-only **ledger of declared intents**, roughly:

```cpp
struct FIntentEntry
{
    TScriptInterface<IWolfCombatant> Combatant;
    FGameplayTag AbilityArchetypeTag;  // the "nature" of the ability — windup, range, damage type — enough for a reactive combatant to judge it without needing the real ability object
    AActor* Target;
    float StartTime;                   // approximate, on the timeline
    EIntentStatus Status;              // Declared / Interrupted / Confirmed
    TOptional<float> UnavailableUntil; // derived from ability metadata — e.g. hitstun window
};
```

This answers two things directly from your message: a reactive combatant queries the ledger for entries belonging to whoever it's watching, reading ability *metadata/tags* — not full simulated state — to judge "the nature of the ability" they're reacting to. And the "which points on the timeline can this combatant act" question is just `UnavailableUntil` on their own most recent entry — published once, checked before offering them a new decision, rather than recomputed from raw simulated state each time.

Whether this needs to be richer than this sketch is still open — flagged below.

---

## 5. Attribute / Damage Preview Model

Carried over from the prior discussion, restated here as part of the architecture rather than a side note: the bake does not and should not apply real `GameplayEffect`s. Predicted damage/health changes should be computed and stored as data on the snapshot (or as a directly-snapshotted attribute value, restored the same way transform/animation already is), read by UI during scrub. Nothing about real GAS effect application, cues, or delegates fires during prediction — only on eventual resolution, whenever that happens.

**Deferred, but flagged so it isn't lost:** ongoing effects with duration (poison/DoTs, HoTs) complicate this, because their contribution to predicted health isn't a single discrete event — it's a running tally that needs to be evaluated at every snapshot along the timeline, not just applied once at the moment the effect starts. Whatever eventually computes "predicted Health at snapshot N" needs to account for standing effects active as of that snapshot's time, not just discrete hit events resolved earlier in the bake. Not a near-term problem, but the eventual predicted-attribute model should be built with this in mind from the start rather than retrofitted, since "point events only" and "point events + ongoing effects" are different enough shapes that bolting the second on late tends to require redoing the first.

---

## 6. Re-Bake on Player Change

Unchanged from `ReBakeTimeline`'s existing shape: restore every tracked combatant to `MasterStartSnapshot`, then re-run the full bake. The orchestrator re-runs for every AI combatant exactly as it did the first time — it does not try to preserve any part of the previous decision tree, because a changed player choice can invalidate downstream AI reasoning in ways that are impractical to detect and patch selectively (an ally's "follow-up" decision doesn't make sense anymore if the move it was following up on changed). Full re-run keeps the system correct and deterministic at the cost of redoing work that's already cheap (this is the same reasoning that made `ReBakeTimeline` the right call for the ability-injection feature).

---

## 7. Non-Goals / Deferred Decisions

- **Pause vs. auto-play at TB entry.** Purely a front-end concern — `CurrentTimelineTime` either auto-advances or doesn't; nothing in the bake/orchestrator cares which. Can be a toggle.
- **Live mutation of the real ASC during scrub.** Rejected as a default (see attribute preview model above) — not revisited here.
- **DoT/HoT-aware predicted attributes.** Flagged in section 5, not designed here.
- **Full roster of interrupt-response types.** The data-driven, weighted table (section 3.2) is the architectural piece; specific responses (dodge, parry, feint) are content and don't need to exist on day one.

---

## 8. Open Questions for Next Pass

1. **Ability metadata.** Nothing in the codebase today exposes windup/active-window/recovery timing or speed as queryable metadata separate from the mechanical `AbilitySequence` a `UBaseCombatAbility` executes. Planning needs this in an abstract, lightweight form to reason about timing without running physics — worth deciding whether it's authored directly on the ability class, derived from the existing `FCombatPeriod` sequence, or a separate data asset.
2. **Iteration bound on interrupt resolution.** Section 3.2 flags that a chosen interrupt response can itself be interrupted. Needs either a hard cap or an authoritative resolution order to guarantee termination.
3. **Round cap for intent negotiation.** How deep can reactive chains go (A reacts to B, C reacts to A's reaction to B...) before the planning phase forces a resolution? Needs a bound for the same reason as above — determinism and termination, not just performance.
4. **Intent Ledger shape.** Section 4's sketch is a starting proposal, not confirmed — may need more than `{Combatant, AbilityArchetypeTag, Target, StartTime, Status, UnavailableUntil}` once real ability content is built against it.
