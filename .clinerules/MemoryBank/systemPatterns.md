# WolfCore System Patterns

## System Architecture

WolfCore is structured as a modular Unreal Engine C++ module with a layered architecture:

```
WolfCore Module
├── Abilities/         - Combat ability implementations (RT, TB, SwitchMode)
├── AbilitySystem/     - GAS wrappers (WolfAbilitySystemComponent, WolfAttributeSet)
├── Character/         - Character base classes (WolfChar, WolfCharacterBase, WolfEnemyBase)
├── Core/              - Core systems (GameMode, GameState, PlayerController, GameInstance)
├── Interfaces/        - Interface definitions (IWolfCombatant, CombatModeListener)
├── Presage/           - Temporal prediction system (ActorSnapshot, PresageAbilityRequest)
├── Systems/           - Subsystems (CombatModeSubsystem)
└── Input/             - Input handling (WolfInputComponent, WolfInputConfig)
```

## Key Technical Decisions

### 1. Snapshot-Based State Management
- Uses `ActorSnapshot` struct (renamed from `ActorState`) to capture actor states
- `TemporalStates` struct (renamed from `MasterSnapshot`) manages snapshot collections
- `ISnapshot` interface with `RestoreSnapshot()` method for state restoration
- `WolfSnapshotComponent` handles snapshot capture and restoration

### 2. Presage Temporal Prediction
- `WolfPresageComponent` - Attached to actors for prediction capabilities
- `WolfPresageSimulator` - Core simulation engine for temporal prediction
- `PresageAbilityRequest` - Request data structure for ability prediction (property renamed: `RequestedTime` → `ScheduledTime`)
- `PresageTimelineEvent` - Timeline event data (properties renamed: `TimeOffset`→`Time`, `Instigator`→`Attacker`, `Target`→`Victim`, `AbilityTag`→`ResultTag`)

### 3. Combat Mode Architecture
- Three modes: OOC (Out of Combat), RT (Real Time), TB (Turn Based)
- `CombatModeSubsystem` manages mode transitions
- Mode-specific time dilation: OOC=1.0, RT=1.0, TB=0.0
- `SwitchMode` ability class (renamed from `GA_SwitchMode`) handles mode changes
- `WolfCombatSettings` configures mode-specific behavior via `DefaultGame.ini`

### 4. Ability System Design
- `WolfAbilitySystemComponent` extends `UAbilitySystemComponent` with input handling
- `WolfAttributeSet` (renamed from `WolfAttributeSetBase`) defines character attributes
- `WolfAbilityComponent` wraps ASC access (property renamed: `AbilitySystemComponent` → `CacheASC`)
- Base combat abilities: `BaseCombatAbility`, `RTCombatAbility`, `TBCombatAbility`
- TB abilities use period-based system: `AbilityPeriodAdvancer`, `OnPeriodFinished`

## Component Relationships

```
WolfChar
├── WolfAbilityComponent (CacheASC → WolfAbilitySystemComponent)
├── WolfAbilitySystemComponent (UAbilitySystemComponent)
│   ├── Input handling (AbilityInputTagPressed/Released/Held)
│   ├── Ability management (AddCharacterAbilities)
│   └── Presage integration (BuildInitialPresageRequest)
├── WolfSnapshotComponent
└── WolfPresageComponent

WolfGameState
├── CombatModeSubsystem
└── Mode state management

WolfPlayerController
└── ApplyCombatMode (renamed from HandleTBTransition/HandleModeTransition)
```

## Design Patterns

### Interface-Based Design
- `IWolfCombatant` - Defines combatant interface for characters
- `ICombatModeListener` / `CombatModeListener` - Mode change notification interface
- `ISnapshot` - Snapshot restoration interface

### Weak Pointer Pattern
- `TWeakObjectPtr<AActor>` used throughout for actor references to prevent circular references
- Presage requests store weak pointers to target actors

### Configuration-Driven
- `AbilityConfig` struct defines ability parameters
- `AbilityFrameData` stores frame-specific data for abilities
- `WolfCombatSettings` (UDeveloperSettings) configures system-wide behavior
- `CharacterStatConfig` defines character-specific stat configurations

### Tag-Based System
- GameplayTags used for ability identification and input mapping
- `InputState.OOC`, `InputState.RT`, `InputState.TB` for mode identification
- `WolfGameplayTags` centralizes tag definitions

## Critical Implementation Paths

### Input → Ability Flow
1. `WolfInputComponent` receives input events
2. `WolfAbilitySystemComponent::AbilityInputTagPressed()` triggered
3. Ability activation request sent to character's ability system
4. If in predictive mode, `BuildInitialPresageRequest()` creates prediction request
5. `WolfPresageSimulator` runs temporal prediction
6. Result validated and ability executed or queued

### Mode Transition Flow
1. `WolfPlayerController::ApplyCombatMode()` called
2. `CombatModeSubsystem` initiates transition
3. Time dilation applied based on target mode
4. `SwitchMode` ability executes
5. `CombatModeListener` interfaces notified

### Snapshot/Restore Flow
1. `WolfSnapshotComponent::CaptureSnapshot()` saves actor state
2. State stored in `ActorSnapshot` struct
3. `TemporalStates` manages snapshot collections
4. `ISnapshot::RestoreSnapshot()` restores captured state