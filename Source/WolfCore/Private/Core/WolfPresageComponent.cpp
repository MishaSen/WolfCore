// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/WolfPresageComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/AbilityPeriodAdvancer.h"
#include "Abilities/BaseCombatAbility.h"
#include "AbilitySystem/WolfAttributeSet.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/WolfCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Core/WolfAbilityComponent.h"
#include "Core/WolfGameplayTags.h"
#include "Debug/WolfDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/IWolfCombatant.h"
#include "Navigation/PathFollowingComponent.h"
#include "DrawDebugHelpers.h"
#include "Misc/TransactionObjectEvent.h"
#include "Presage/PresageImpactLedger.h"
#include "Core/WolfSnapshotComponent.h"
#include "Core/WolfResourceRules.h"
#include "Systems/CombatModeSubsystem.h"

UWolfPresageComponent::UWolfPresageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWolfPresageComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<AWolfCharacterBase>(GetOwner());
	if (CharacterOwner)
	{
		CachedASC = Cast<UWolfAbilitySystemComponent>(CharacterOwner->GetAbilitySystemComponent());
		AbilityControl = CharacterOwner->FindComponentByClass<UWolfAbilityComponent>();
	}
}

void UWolfPresageComponent::SimulateTick(float Step)
{
	if (!CharacterOwner) return;

	SimElapsedTime += Step;
	TryActivateNextPlannedIntent();

	SimulatePhysicsStep(Step);
	SimulateAnimationStep(Step);

	// Advance period BEFORE snapshot capture so that both CreateSnapshot and
	// CurrentPeriodIndex assignment reflect the same post-advancement state.
	SimPeriodTime += Step;
	if (auto* ActiveAbility = GetActiveSimulationAbility())
	{
		const int32 PeriodBeforeAdvance = ActiveAbility->GetCurrentPeriodIndex();

		// Captured before the exhaustion/interrupt logic below can advance NextPlannedIntentIndex,
		// so impact-target resolution further down still sees the entry that was actually active
		// during this tick's advancement, not whatever comes next.
		const int32 EntryIndexForThisTick = NextPlannedIntentIndex;

		SimPeriodTime = FAbilityPeriodAdvancer::AdvancePeriod(
			ActiveAbility,
			SimPeriodTime,
			GetMoveComp()->MaxWalkSpeed,
			GetMoveComp()->MaxAcceleration,
			GetMoveComp()->Velocity.Size(),
			CharacterOwner->GetActorLocation()
		);

		// If this was our own planned/simulated ability (not a real one carried over from RT) and
		// its sequence just ran out, OR it's been interrupted and reached its cutoff time, free it
		// up so the next planned intent can take over.
		if (ActiveAbility == SimulatedAbility)
		{
			bool bExhausted = !ActiveAbility->GetAbilitySequence().IsValidIndex(ActiveAbility->GetCurrentPeriodIndex());

			bool bInterruptedNow = false;
			if (PlannedIntents.IsValidIndex(NextPlannedIntentIndex))
			{
				const FIntentEntry& CurrentEntry = PlannedIntents[NextPlannedIntentIndex];
				if (CurrentEntry.bHasInterruptedAtTime && SimElapsedTime >= CurrentEntry.InterruptedAtTime)
				{
					bInterruptedNow = true;
				}
			}

			if (bExhausted || bInterruptedNow)
			{
				bSimulatedAbilityActive = false;
				++NextPlannedIntentIndex;
			}
		}

		// --- Impact detection (PresagePreviewStage2): resolve at most one hit per period per
		// bake, when SimPeriodTime crosses the period's impact offset. Uses the exact same timing
		// source CorrectnessPunchList item 3 established — do not re-derive it here. ---
		if (const int32 CurrentPeriodIndex = ActiveAbility->GetCurrentPeriodIndex();
			ActiveAbility->GetAbilitySequence().IsValidIndex(CurrentPeriodIndex))
		{
			if (CurrentPeriodIndex != LastSimulatedPeriodIndex)
			{
				bCurrentPeriodImpactResolved = false;
				LastSimulatedPeriodIndex = CurrentPeriodIndex;
			}

			const FCombatPeriod& CurrentPeriodData = ActiveAbility->GetAbilitySequence()[CurrentPeriodIndex];
			if (!bCurrentPeriodImpactResolved && CurrentPeriodData.Type == EPeriodType::Attack)
			{
				const float ImpactOffset = UBaseCombatAbility::GetPeriodImpactOffset(CurrentPeriodData, ActiveAbility->HitEventTag);
				if (SimPeriodTime >= ImpactOffset)
				{
					ResolveSimulatedImpact(ActiveAbility, CurrentPeriodData, EntryIndexForThisTick);
					bCurrentPeriodImpactResolved = true;
				}
			}
		}
	}

	FActorSnapshot FutureFrame;
	if (auto* SnapshotControl = GetSnapshotControl())
	{
		ISnapshot::Execute_CreateSnapshot(SnapshotControl, FutureFrame);
	}

	FutureFrame.ActiveAbility = GetActiveSimulationAbility();
	FutureFrame.CurrentPeriodIndex = FutureFrame.ActiveAbility.IsValid()
		? FutureFrame.ActiveAbility->GetCurrentPeriodIndex()
		: -1;

	/*WOLF_LOG(Log, TEXT("[STEP %d] Character: %s | Location: %s| Ability: %s | Period: %d | Montage: %s (Pos: %.2f)"),
		PredictionBuffer.Num(),
		*CharacterOwner.GetName(),
		*FutureFrame.Location.ToCompactString(),
		FutureFrame.ActiveAbility.IsValid() ? *FutureFrame.ActiveAbility->GetName() : TEXT("None"),
		FutureFrame.CurrentPeriodIndex,
		FutureFrame.CurrentMontage.IsValid() ? *FutureFrame.CurrentMontage->GetName() : TEXT("None"),
		FutureFrame.MontagePosition);*/
	PredictionBuffer.Add(FutureFrame); // Remember to clear PredictionBuffer in CombatModeSubsystem
}

void UWolfPresageComponent::CaptureCurrentFrame()
{
	if (!CharacterOwner) return;

	FActorSnapshot Frame;
	if (auto* SnapshotControl = GetSnapshotControl())
	{
		ISnapshot::Execute_CreateSnapshot(SnapshotControl, Frame);
	}

	if (auto* ActiveAbility = GetActiveSimulationAbility())
	{
		Frame.ActiveAbility = ActiveAbility;
		Frame.CurrentPeriodIndex = ActiveAbility->GetCurrentPeriodIndex();
	}
	else
	{
		Frame.ActiveAbility = nullptr;
		Frame.CurrentPeriodIndex = -1;
	}

	PredictionBuffer.Add(Frame);
}

void UWolfPresageComponent::ClearPredictionBuffer(float MaxDuration)
{
	const float StepSize = GetCMS() ? GetCMS()->GetBakedStepSize() : 0.1f; // Fallback only; GetCMS() should not normally be null.
	const int32 ExpectedFrames = FMath::CeilToInt(MaxDuration / StepSize);
	PredictionBuffer.Empty(ExpectedFrames + 1); // +1 for the t=0 frame captured by CaptureCurrentFrame().
}

const FActorSnapshot* UWolfPresageComponent::GetSnapshotAtTime(float RelativeTime) const
{
	if (PredictionBuffer.Num() == 0) return nullptr;

	// Read the step size from the subsystem that owns the bake-time invariant.
	// This ensures index math uses the actual step size used during ExecuteFutureBake,
	// not a compile-time constant that could drift out of sync if the simulator changes.
	const float StepSize = GetCMS() ? GetCMS()->GetBakedStepSize() : 0.1f; // Fallback only; GetCMS() should not normally be null.
	const int32 Index = FMath::Clamp(
		FMath::RoundToInt(RelativeTime / StepSize),
		0,
		PredictionBuffer.Num() - 1);
	return &PredictionBuffer[Index];
}

void UWolfPresageComponent::SimulatePhysicsStep(float Step)
{
	if (!GetCMS() || !GetMoveComp()) return;

	FVector Destination;
	const auto SimVelocity = GetSimulatedVelocity(Destination);
	
	if (SimVelocity.IsNearlyZero()) return; // Return early if simulated velocity is negligible.

	const FVector Start = CharacterOwner->GetActorLocation();
	FVector Delta = SimVelocity * Step;
	// Not considering starting acceleration, but might not make a difference. Look out for bugs.
	
	if (!Destination.IsZero())
	{
		const float DistanceToTarget = FVector::Dist(Start, Destination);
		if (Delta.Size() > DistanceToTarget) Delta = Delta.GetSafeNormal() * DistanceToTarget;
	}
	const FVector End = Start + Delta;
	ResolveMovementWithCollision(Start, End, Delta);
}

FVector UWolfPresageComponent::GetSimulatedVelocity(FVector& Destination) const
{
	const FVector Velocity = GetMoveComp()->Velocity;
	
	if (const auto* Ability = GetActiveSimulationAbility())
	{
		const auto& Sequence = Ability->GetAbilitySequence();
		const auto Index = Ability->GetCurrentPeriodIndex();
		if (Sequence.IsValidIndex(Index) && Sequence[Index].Type == EPeriodType::MoveTo)
		{
			Destination = Sequence[Index].MoveToDestination;
			const FVector ToDestination = Destination - CharacterOwner->GetActorLocation();
			return ToDestination.GetSafeNormal() * GetMoveComp()->MaxWalkSpeed;
		}
	}
	return Velocity;
}

void UWolfPresageComponent::ResolveMovementWithCollision(const FVector& Start, const FVector& End, FVector& Delta)
{
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);
	
	const FQuat Rotation = CharacterOwner->GetActorQuat();
	const FCollisionShape Shape = CharacterOwner->GetCapsuleComponent()->GetCollisionShape();
	
	FHitResult Hit(1.f);
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, Rotation,ECC_Pawn, Shape, Params);
	if (!bHit) { CharacterOwner->SetActorLocation(End, false, nullptr, ETeleportType::TeleportPhysics); return; }

	const FVector RemainingDelta = Delta * (1.f - Hit.Time);
	const FVector SlideDelta = FVector::VectorPlaneProject(RemainingDelta, Hit.Normal);
	if (SlideDelta.IsNearlyZero()) { CharacterOwner->SetActorLocation(Hit.Location, false, nullptr, ETeleportType::TeleportPhysics); return; }

	FHitResult SlideHit;
	const FVector SlideEnd = Hit.Location + SlideDelta;
	const bool bSlideHit = GetWorld()->
		SweepSingleByChannel(SlideHit,Hit.Location, SlideEnd, Rotation,ECC_Pawn, Shape, Params);
	
	CharacterOwner->SetActorLocation(bSlideHit ? SlideHit.Location : SlideEnd, false, nullptr, ETeleportType::TeleportPhysics);
}

void UWolfPresageComponent::SimulateAnimationStep(float DeltaTime)
{
	auto* AnimInst = GetAnimInst();
	if (!IsValid(AnimInst)) return;
	
	const auto* CurrentMontage = GetCurrentMontage();
	if (!IsValid(CurrentMontage)) return;

	const auto CurrentPos = AnimInst->Montage_GetPosition(CurrentMontage);
	const auto NewPos = CurrentPos + DeltaTime;

	AnimInst->Montage_SetPosition(CurrentMontage, NewPos);

	if (CurrentMontage->HasRootMotion())
	{
		const auto RootMotionDelta = CurrentMontage->ExtractRootMotionFromRange(CurrentPos,NewPos, FAnimExtractContext());
		const auto WorldDelta = GetOwnerRotation().RotateVector(RootMotionDelta.GetLocation());

		CharacterOwner->AddActorWorldOffset(WorldDelta, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (NewPos >= CurrentMontage->GetPlayLength())
	{
		AnimInst->Montage_Stop(0.1f, CurrentMontage); // If seeing t-poses, set to idle state
	}
}

FVector UWolfPresageComponent::GetOwnerLocation() const { return CharacterOwner ? CharacterOwner->GetActorLocation() : FVector::ZeroVector; }
FRotator UWolfPresageComponent::GetOwnerRotation() const { return CharacterOwner ? CharacterOwner->GetActorRotation() : FRotator::ZeroRotator; }

UCharacterMovementComponent* UWolfPresageComponent::GetMoveComp() const { return CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr; }
UWolfAbilitySystemComponent* UWolfPresageComponent::GetASC() const { return CachedASC; }
UAnimInstance* UWolfPresageComponent::GetAnimInst() const { return CharacterOwner ? CharacterOwner->GetAnimInst() : nullptr; }
UAnimMontage* UWolfPresageComponent::GetCurrentMontage() const { return GetAnimInst() ? GetAnimInst()->GetCurrentActiveMontage() : nullptr; }
UCombatModeSubsystem* UWolfPresageComponent::GetCMS() const { return CharacterOwner ? CharacterOwner->GetCMS() : nullptr; }

UWolfSnapshotComponent* UWolfPresageComponent::GetSnapshotControl() const
{
	return CharacterOwner ? CharacterOwner->GetSnapshotComponent() : nullptr;
}

void UWolfPresageComponent::SetInjectedAbilityRequest(const FPresageAbilityRequest& Request)
{
	InjectedAbilityRequest = Request;
}

void UWolfPresageComponent::ClearInjectedAbilityRequest()
{
	InjectedAbilityRequest = FPresageAbilityRequest();
}

void UWolfPresageComponent::SetPlannedIntents(const TArray<FIntentEntry>& Intents)
{
	PlannedIntents = Intents;
	NextPlannedIntentIndex = 0;
}

void UWolfPresageComponent::BeginSimulation()
{
	SimElapsedTime = 0.f;
	SimPeriodTime = 0.f;
	bSimulatedAbilityActive = false;
	LastSimulatedPeriodIndex = -1;
	bCurrentPeriodImpactResolved = false;
	SimulatedAbility = nullptr;
}

void UWolfPresageComponent::EndSimulation()
{
	SimulatedAbility = nullptr;
	bSimulatedAbilityActive = false;
	LastSimulatedPeriodIndex = -1;
	ClearInjectedAbilityRequest();
}

UBaseCombatAbility* UWolfPresageComponent::GetActiveSimulationAbility() const
{
	if (!CharacterOwner) return nullptr;

	if (auto* RealAbility = CharacterOwner->GetActiveCombatAbility())
	{
		if (RealAbility->GetAbilitySequence().IsValidIndex(RealAbility->GetCurrentPeriodIndex()))
		{
			return RealAbility;
		}
		// RealAbility's simulated sequence has run its course for this bake — fall through so an
		// injected ability (if any) can take over instead of being permanently blocked.
	}

	if (bSimulatedAbilityActive && SimulatedAbility)
	{
		return SimulatedAbility;
	}

	return nullptr;
}

void UWolfPresageComponent::TryActivateNextPlannedIntent()
{
	if (bSimulatedAbilityActive) return;
	if (!PlannedIntents.IsValidIndex(NextPlannedIntentIndex)) return;

	const FIntentEntry& Entry = PlannedIntents[NextPlannedIntentIndex];
	if (SimElapsedTime < Entry.StartTime) return;
	if (!Entry.AbilityClass) return;

	SimulatedAbility = NewObject<UBaseCombatAbility>(
		this,
		Entry.AbilityClass,
		NAME_None,
		RF_Transient);

	const auto* AbilityCDO = Entry.AbilityClass->GetDefaultObject<UBaseCombatAbility>();
	TArray<FCombatPeriod> Sequence = AbilityCDO ? AbilityCDO->GetAbilitySequence() : TArray<FCombatPeriod>();

	UBaseCombatAbility::ResolveMoveToDestinations(
		Sequence,
		CharacterOwner->GetActorLocation(),
		Entry.Target.Get());

	SimulatedAbility->InitializeForSimulation(Sequence);
	bSimulatedAbilityActive = true;
	LastSimulatedPeriodIndex = 0;
	SyncSimulationMontage(SimulatedAbility);

	WOLF_LOG(Log, TEXT("[PRESAGE] Activated planned intent %d/%d: %s at t=%.2fs for %s"),
		NextPlannedIntentIndex + 1, PlannedIntents.Num(),
		*Entry.AbilityClass->GetName(), SimElapsedTime, *CharacterOwner->GetName());
}

void UWolfPresageComponent::ResolveSimulatedImpact(UBaseCombatAbility* ActiveAbility, const FCombatPeriod& Period, int32 EntryIndexForThisTick)
{
	if (!CharacterOwner) return;

	auto* CMS = GetCMS();
	if (!CMS) return;

	AActor* Attacker = CharacterOwner;

	// Victim resolution: a planned intent carries its own Target. A carried-over real ability
	// (not our own SimulatedAbility) has no intent entry, so fall back to GatherPresageTargets —
	// the same resolution real RT abilities use.
	AActor* Victim = nullptr;
	if (ActiveAbility == SimulatedAbility && PlannedIntents.IsValidIndex(EntryIndexForThisTick))
	{
		Victim = PlannedIntents[EntryIndexForThisTick].Target.Get();
	}
	else
	{
		for (const auto& WeakTarget : CharacterOwner->GatherPresageTargets())
		{
			if (WeakTarget.IsValid())
			{
				Victim = WeakTarget.Get();
				break;
			}
		}
	}

	FPresageImpactEntry Entry;
	Entry.Attacker = Attacker;
	Entry.Victim = Victim;
	Entry.ImpactTime = SimElapsedTime;

	// Connection rule (deliberately simple this stage): victim valid, within Period.Range, and
	// not evading (its own active sim period is EPeriodType::Evasion) at this moment.
	// KNOWN, ACCEPTED APPROXIMATION: combatants advance in fixed TrackedCombatants order within
	// one step, so an attacker earlier in the array reads victims at the previous step's
	// position — up to one step (default 0.1s) of skew. This is deterministic (fixed order, fixed
	// step), which is what the exact-preview contract requires; sub-step precision is a tuning
	// concern, not a correctness one. Do not "fix" this into nondeterminism.
	bool bConnected = false;
	if (Victim)
	{
		const float Distance = FVector::Dist(CharacterOwner->GetActorLocation(), Victim->GetActorLocation());
		bConnected = Distance <= Period.Range;

		if (bConnected)
		{
			auto* VictimCombatant = Cast<IWolfCombatant>(Victim);
			auto* VictimPresage = VictimCombatant ? VictimCombatant->GetPresageComponent() : nullptr;
			if (VictimPresage)
			{
				if (const auto* VictimAbility = VictimPresage->GetActiveSimulationAbility())
				{
					const auto& VictimSequence = VictimAbility->GetAbilitySequence();
					const int32 VictimPeriodIndex = VictimAbility->GetCurrentPeriodIndex();
					if (VictimSequence.IsValidIndex(VictimPeriodIndex) &&
						VictimSequence[VictimPeriodIndex].Type == EPeriodType::Evasion)
					{
						bConnected = false;
					}
				}
			}
		}
	}
	Entry.bConnected = bConnected;

	if (Victim)
	{
		const auto& WolfTag = FWolfGameplayTags::Get();
		const auto* VictimPawn = Cast<APawn>(Victim);
		const bool bVictimIsPlayer = VictimPawn && VictimPawn->IsPlayerControlled();

		auto* VictimASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Victim);
		const bool bVictimIsLinked = VictimASC && VictimASC->HasMatchingGameplayTag(WolfTag.Status_Link);

		Entry.bVictimIsPlayerOrLinked = bVictimIsPlayer || bVictimIsLinked;
	}

	// Planned intents have no live GameplayAbilitySpec (SimulatedAbility is never actually
	// GAS-activated) — there is no real level to read. Treat as level 1, matching the CDO
	// default. Revisit if ability levels become dynamic.
	constexpr float SimulatedAbilityLevel = 1.f;

	for (const auto& Effect : Period.HitEffects)
	{
		if (!Effect.EffectClass) continue;

		FPredictedEffectDelta Delta;
		Delta.EffectClass = Effect.EffectClass;
		Delta.bSelfTarget = Effect.bSelfTarget;

		if (!Effect.TargetAttributeTag.IsValid())
		{
			Delta.bIsPredictable = false;
			WOLF_WARN(TEXT("[PRESAGE] %s's FCombatHitEffect (%s) has no TargetAttributeTag — magnitude-predictability rule violated, prediction skipped for this entry."),
				*ActiveAbility->GetName(), *Effect.EffectClass->GetName());
			Entry.Deltas.Add(Delta);
			continue;
		}

		const auto Attribute = UWolfAttributeSet::GetAttributeByTag(Effect.TargetAttributeTag);
		if (!Attribute.IsValid())
		{
			Delta.bIsPredictable = false;
			WOLF_WARN(TEXT("[PRESAGE] TargetAttributeTag %s did not resolve to a known attribute — prediction skipped."),
				*Effect.TargetAttributeTag.ToString());
			Entry.Deltas.Add(Delta);
			continue;
		}

		Delta.Attribute = Attribute;
		Delta.Amount = Effect.Amount.GetValueAtLevel(SimulatedAbilityLevel) * Effect.SignMultiplier;
		Delta.bIsPredictable = true;
		Entry.Deltas.Add(Delta);

		// Apply immediately, numerically, via SetNumericAttributeBase — never through real GE
		// application (no PostGameplayEffectExecute, no Die(); predicted death is health 0 in
		// data, per stage 1's contract). Subsequent SimulateTick frames snapshot the mutated
		// attributes automatically — no snapshot-path changes needed.
		if (Entry.bConnected)
		{
			AActor* ApplyTarget = Effect.bSelfTarget ? Attacker : Victim;
			if (ApplyTarget)
			{
				auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ApplyTarget);
				if (TargetASC)
				{
					float NewValue = TargetASC->GetNumericAttribute(Attribute) + Delta.Amount;

					// Replicate PreAttributeChange's clamps explicitly: floor 0 always; Health
					// additionally capped at MaxHealth. SetNumericAttributeBase still invokes
					// PreAttributeChange, but this is verified here rather than assumed.
					NewValue = FMath::Max(NewValue, 0.f);
					if (Attribute == UWolfAttributeSet::GetHealthAttribute())
					{
						NewValue = FMath::Min(NewValue, TargetASC->GetNumericAttribute(UWolfAttributeSet::GetMaxHealthAttribute()));
					}

					TargetASC->SetNumericAttributeBase(Attribute, NewValue);
				}
			}
		}
	}

	// [ResourceLoopStage1] Resource gains — BAKED (prediction) path.
	//
	// Same pure FWolfResourceRules::ComputeResourceGain as the live paths. The mode is
	// hard-coded to TB because the bake only ever simulates a TB session — passing it as a
	// parameter rather than reading the live CMS is exactly the pure-function contract from
	// ResourceLoopStage1.md, and it is what keeps predicted gains identical to the real ones
	// execution applies.
	if (Entry.bConnected)
	{
		const auto* AttackerPawn = Cast<APawn>(Attacker);
		const auto* VictimPawn = Cast<APawn>(Victim);
		// TODO(teams): "player-side dealt/taken" replaces "player dealt/taken" here when a team
		// filter exists (see ResourceLoopStage1.md).
		const bool bPlayerDealtHit = AttackerPawn && AttackerPawn->IsPlayerControlled();
		const bool bPlayerTookHit = VictimPawn && VictimPawn->IsPlayerControlled();

		// Primary damage magnitude — the first predictable delta's absolute value, mirroring the
		// RT site (period's first hit effect's magnitude).
		float DamageAmount = 0.f;
		for (const auto& Delta : Entry.Deltas)
		{
			if (Delta.bIsPredictable)
			{
				DamageAmount = FMath::Abs(Delta.Amount);
				break;
			}
		}

		const auto ResourceGain = FWolfResourceRules::ComputeResourceGain(
			bPlayerDealtHit, bPlayerTookHit, DamageAmount, FWolfGameplayTags::Get().InputState_TB);

		// Record for exact replay: execution applies these recorded values (ApplyDueLedgerImpacts).
		Entry.FlowGain = ResourceGain.FlowDelta;
		Entry.AdrenalineGain = ResourceGain.AdrenalineDelta;

		// Preview integration: mirror the gains into the player's attributes NUMERICALLY via
		// SetNumericAttributeBase — never through a real GE (no PostGameplayEffectExecute, no
		// telemetry spam, per the exact-preview contract) — so scrubbing the plan shows the
		// player's resource building as it will really happen. Real application is deferred to
		// execution playback via the recorded values above.
		if (UAbilitySystemComponent* PlayerASC = CMS->GetPlayerASC())
		{
			if (!FMath::IsNearlyZero(ResourceGain.FlowDelta))
			{
				float NewFlow = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetFlowGaugeAttribute()) + ResourceGain.FlowDelta;
				PlayerASC->SetNumericAttributeBase(UWolfAttributeSet::GetFlowGaugeAttribute(), FMath::Max(NewFlow, 0.f));
			}
			if (!FMath::IsNearlyZero(ResourceGain.AdrenalineDelta))
			{
				float NewAdrenaline = PlayerASC->GetNumericAttribute(UWolfAttributeSet::GetAdrenalineAttribute()) + ResourceGain.AdrenalineDelta;
				PlayerASC->SetNumericAttributeBase(UWolfAttributeSet::GetAdrenalineAttribute(), FMath::Max(NewAdrenaline, 0.f));
			}
		}
	}

	CMS->AppendImpactLedgerEntry(Entry);
}

void UWolfPresageComponent::SyncSimulationMontage(UBaseCombatAbility* Ability)
{
	if (!Ability) return;

	const auto& Sequence = Ability->GetAbilitySequence();
	const int32 Index = Ability->GetCurrentPeriodIndex();
	if (!Sequence.IsValidIndex(Index)) return;

	const auto& Period = Sequence[Index];
	if (!IsValid(Period.Montage)) return;

	if (auto* AnimInst = GetAnimInst())
	{
		AnimInst->Montage_Play(Period.Montage);
		AnimInst->Montage_SetPosition(Period.Montage, 0.f);
	}
}
