# WolfCore Tech Context

## Technologies Used
- **Unreal Engine 5.6** - Game engine framework
- **C++** - Primary development language
- **Gameplay Ability System (GAS)** - Core ability and attribute framework
- **Enhanced Input System** - Input handling and mapping
- **Blueprint** - Asset-based prototyping and character configuration

## Development Setup
- **IDE**: JetBrains Rider
- **Platform**: Windows 11
- **Build System**: Unreal Build Tool (UBT) with .Build.cs configuration
- **Target Platforms**: Windows DX12 (primary), DX11 (secondary)
- **Shader Compilation**: SM5 and SM6 support enabled

## Technical Constraints
- Module must use WOLFCORE_API export macro for DLL visibility
- Blueprint classes use `_C` suffix convention for runtime references
- Time dilation is mode-specific: OOC=1.0, RT=1.0, TB=0.0 (paused during turn-based)
- Default map: `/Game/WolfCore/Maps/MyLevel`
- Default game mode: `BP_WolfGameMode_C` (Blueprint)
- Game instance: `WolfGameInstance` (C++)
- Asset manager: `WolfAssetManager` (C++)

## Dependencies

### Engine Plugins
- **GameplayAbilities** - GAS core functionality
- **GameplayStateTree** - State tree AI integration
- **AIModule** - AI behavior frameworks (Editor only: ModelingToolsEditorMode)

### Build Dependencies (WolfCore.Build.cs)
- Engine
- AIModule
- GameplayAbilities
- EnhancedInput

### External MCP Servers

| Server | Purpose |
|--------|---------|
| github.com/modelcontextprotocol/servers/tree/main/src/filesystem | Filesystem access (working directory: C:/Wolf/WolfCore) |
| github.com/modelcontextprotocol/servers/tree/main/src/sequentialthinking | Chain-of-thought reasoning |
| github.com/cline/linear-mcp | Linear project management (issues, projects, teams, comments, milestones) |

## Configuration Files

### DefaultEngine.ini
- Game/Editor default map: `/Game/WolfCore/Maps/MyLevel`
- Global game mode: `BP_WolfGameMode_C`
- Game instance: `WolfGameInstance`
- Asset manager: `WolfAssetManager`
- Renderer: Virtual shadow maps, ray tracing enabled
- CoreRedirects: Class/property renames for backward compatibility

### DefaultGame.ini
- GameplayCueNotifyPaths: `/Game/WolfCore`
- Debug target HUD: enabled
- WolfCombatSettings: Presage effect class, mode time dilation map

### DefaultInput.ini
- Default player input: `EnhancedPlayerInput`
- Default input component: `WolfInputComponent`
- Full controller/VR axis mappings configured

## Key Classes and Their Roles

| Class | Base Class | Purpose |
|-------|-----------|---------|
| WolfChar | ACharacter | Main character implementation |
| WolfCharacterBase | ACharacter | Base character with ASC integration |
| WolfEnemyBase | WolfCharacterBase | Enemy-specific character logic |
| WolfAbilitySystemComponent | UAbilitySystemComponent | Input handling + ability management |
| WolfAttributeSet | UAttributeSet | Character attribute definitions |
| WolfAbilityComponent | UActorComponent | ASC wrapper with CacheASC |
| WolfGameState | AGameStateBase | Game state with combat mode |
| WolfGameMode | AGameModeBase | Game mode base (legacy, flagged for deletion) |
| WolfPlayerController | APlayerController | Mode transitions, ApplyCombatMode |
| WolfGameInstance | UGameInstance | Persistent game state |
| WolfAssetManager | UAssetManager | Asset loading management |
| WolfPresageComponent | UActorComponent | Temporal prediction on actors |
| WolfPresageSimulator | (Core) | Simulation engine |
| WolfSnapshotComponent | UActorComponent | State capture/restore |
| CombatModeSubsystem | UGameplaySubsystem | Mode management |
| WolfInputComponent | UInputComponent | Enhanced input wrapper |
| WolfInputConfig | (Config) | Input mapping configuration |
| WolfCombatSettings | UDeveloperSettings | System-wide combat config |
| WolfFunctionLibrary | (Static) | Utility functions |
| WolfGameplayTags | (Static) | GameplayTag definitions |
| WolfLevelScript | ULevelScriptBlueprint | Level combat mode bridge |
| WolfDebug | (Static) | Debug utilities |

## Naming Conventions

### Class Renames (tracked in CoreRedirects)
- `WolfCharacter` → `WolfChar`
- `WolfAttributeSetBase` → `WolfAttributeSet`
- `WolfGameModeBase` → `WolfGameMode`
- `WolfGameStateBase` → `WolfGameState`
- `GA_SwitchMode` → `SwitchMode`
- `ActorState` → `ActorSnapshot`
- `MasterSnapshot` → `TemporalStates`
- `LevelCombatMode` → `WolfLevelScript`

### Property Renames (tracked in CoreRedirects)
- `AtSet` → `AttributeSet` (WolfCharacterBase)
- `ASet` → `AttributeSet` (WolfPlayerState)
- `RequestedTime` → `ScheduledTime` (PresageAbilityRequest)
- `TimeOffset` → `Time` (PresageTimelineEvent)
- `Instigator` → `Attacker` (PresageTimelineEvent)
- `Target` → `Victim` (PresageTimelineEvent)
- `AbilityTag` → `ResultTag` (PresageTimelineEvent)
- `Character` → `TrackCharacter` (CharacterTimelineTrack)
- `AbilitySystemComponent` → `CacheASC` (WolfAbilityComponent)
- `AIMoveTarget` → `Destination` (ActorSnapshot)

### Function Renames (tracked in CoreRedirects)
- `HandleTBTransition` → `HandleModeTransition` → `ApplyCombatMode` (WolfPlayerController)
- `OnDelayFinished` → `OnPeriodFinished` (TBCombatAbility)
- `RestoreSnapshot_Implementation` → `RestoreSnapshot` (ISnapshot)