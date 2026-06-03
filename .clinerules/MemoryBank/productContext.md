# WolfCore Product Context

## Why This Project Exists
WolfCore exists to provide a robust, modular combat framework for games that require precise combat mechanics with temporal prediction. It addresses the need for:
- Predictive combat systems that can simulate outcomes before committing to them
- A unified abstraction layer over Unreal Engine's Gameplay Ability System (GAS)
- Support for both real-time and turn-based combat paradigms within the same architecture

## Problems It Solves
1. **Combat Prediction**: The Presage system enables simulating ability outcomes before actual execution, critical for turn-based and prediction-based gameplay
2. **State Management**: Snapshot-based architecture enables rollback, undo, and deterministic simulation capabilities
3. **Mode Flexibility**: Single codebase supports multiple combat modes (OOC, RT, TB) with mode-specific behaviors
4. **GAS Abstraction**: Simplifies the complexity of Unreal's GAS while maintaining full functionality

## How It Should Work

### Combat Flow
1. Player inputs actions through Enhanced Input system
2. Input triggers ability requests on the WolfAbilitySystemComponent
3. For predictive modes, Presage system simulates the ability outcome first
4. Results are validated and either executed immediately (RT) or queued (TB)
5. State snapshots enable rollback if predictions don't match reality

### Combat Modes
- **Out of Combat (OOC)**: Default state, basic abilities only, no combat simulation
- **Real Time (RT)**: Standard real-time combat with immediate ability execution, time dilation support
- **Turn Based (TB)**: Turn-based combat with delays, periods, and predictive resolution

### Key Systems
- **Presage**: Temporal prediction engine that simulates combat outcomes at hypothetical future timestamps
- **Snapshot System**: Captures and restores actor states for rollback/undo functionality
- **Combat Mode Subsystem**: Manages transitions between OOC, RT, and TB modes
- **Ability System**: Custom wrapper around GAS providing unified ability management

## User Experience Goals
- Characters should respond to input with minimal latency in RT mode
- Turn-based combat should feel structured with clear turn order and timing
- Ability effects should be predictable and consistent between prediction and execution
- System should be extensible for adding new abilities and combat mechanics