# WolfCore Active Context

## Current Work Focus
Memory Bank initialization for WolfCore project. All core documentation files are being created to establish the knowledge base for future development sessions.

## Recent Changes
- Initial Memory Bank setup completed
- Core documentation files created: all 6 core files (projectbrief.md, productContext.md, activeContext.md, systemPatterns.md, techContext.md, progress.md)
- Linear MCP server installed at C:\Users\Shane\Documents\Cline\MCP\linear-mcp
- Connected to Linear team "MishaNes" (key: MIS)
- Server name in settings: github.com/cline/linear-mcp
- Fixed TB → RT input controls stuck bug (commit: 57de1857d4f37e2b7f7b941d84a2452ebb9d5e28)

## Next Steps
- Complete remaining Memory Bank files: systemPatterns.md, techContext.md, progress.md
- Document key class relationships and architecture patterns
- Record technical constraints and development setup details
- Test TB → RT input fix in-editor

## Active Decisions and Considerations
- Using snapshot-based architecture (ActorSnapshot, TemporalStates) instead of older ActorState naming
- Presage system renamed from older naming conventions (PresageMode, PresageAbilityRequest)
- Character classes renamed: WolfCharacter → WolfChar, WolfAttributeSetBase → WolfAttributeSet
- Game mode renamed: WolfGameModeBase → WolfGameMode, WolfGameStateBase → WolfGameState
- TB combat ability uses "Period" terminology instead of "Delay" (OnPeriodFinished, AbilityPeriodAdvancer)

## Important Patterns and Preferences
- All classes use WOLFCORE_API export macro for module DLL visibility
- Blueprint-native classes use `_C` suffix convention
- GameplayTags used extensively for ability identification and input mapping
- Weak object pointers (TWeakObjectPtr) used for actor references to avoid circular references
- Interface-based design for combatant and mode listener patterns
- External MCP servers stored at C:\Users\Shane\Documents\Cline\MCP\
- Linear API key authentication used for project management integration

## Learnings and Project Insights
- System uses CoreRedirects in DefaultEngine.ini for class/property renames rather than refactoring
- Time dilation is mode-specific: OOC=1.0, RT=1.0, TB=0.0 (paused during turn-based)
- Default map and game modes are configured via Blueprint (BP_WolfGameMode_C, WolfGameInstance, WolfAssetManager)
- Custom input component (WolfInputComponent) extends EnhancedInput system