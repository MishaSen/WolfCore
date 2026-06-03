# WolfCore Progress

## What Works
- Core module structure with WOLFCORE_API export macro
- Gameplay Ability System integration (WolfAbilitySystemComponent, WolfAttributeSet)
- Character framework (WolfChar, WolfCharacterBase, WolfEnemyBase)
- Combat mode system (OOC, RT, TB) with CombatModeSubsystem
- Enhanced Input integration via WolfInputComponent
- Presage temporal prediction system (WolfPresageComponent, WolfPresageSimulator)
- Snapshot-based state management (WolfSnapshotComponent, ActorSnapshot, ISnapshot)
- Mode-specific time dilation configuration
- CoreRedirects for backward compatibility with renamed classes/properties/functions

## What's Left to Build
- Full ability implementation library (RTCombatAbility, TBCombatAbility base classes ready)
- Complete Presage simulation logic
- Turn-based combat turn order and queue system
- Blueprint character configurations (BP_WolfGameMode_C exists, needs full character blueprints)
- Content assets (abilities, effects, cues)
- Testing and validation framework

## Current Status
- Memory Bank initialized with all 6 core documentation files
- Project structure documented with all key classes and relationships
- CoreRedirects mapping preserves backward compatibility for naming changes

## Known Issues
- `WolfGameMode` class flagged for deletion (legacy, replaced by Blueprint-based `BP_WolfGameMode_C`)
- Some classes still use legacy naming (redirected via CoreRedirects rather than refactored)
- Content directory appears sparse - may need additional game content development

## Evolution of Project Decisions
1. **Naming Convention Changes**: Multiple class renames tracked via CoreRedirects (WolfCharacter→WolfChar, WolfAttributeSetBase→WolfAttributeSet, etc.)
2. **Presage System Evolution**: Renamed from older naming conventions, properties updated (RequestedTime→ScheduledTime, TimeOffset→Time, etc.)
3. **TB Ability Terminology**: Changed from "Delay" to "Period" (OnDelayFinished→OnPeriodFinished, GA_SwitchMode→SwitchMode)
4. **Snapshot Architecture**: Evolved from ActorState/MasterSnapshot to ActorSnapshot/TemporalStates naming
5. **Blueprint vs C++**: Game mode and character configuration pushed to Blueprints while core systems remain in C++