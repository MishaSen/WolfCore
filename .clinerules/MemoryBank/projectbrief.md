# WolfCore Project Brief

## Overview
WolfCore is a custom Unreal Engine 5.6 C++ game engine module built on top of the Gameplay Ability System (GAS). It serves as a foundational framework for a combat-focused game with temporal prediction and snapshot-based combat resolution.

## Core Goals
- Implement a modular combat system using Unreal Engine's Gameplay Ability System
- Provide temporal prediction capabilities through the "Presage" system for predicting combat outcomes before they execute
- Support multiple combat modes (RT - Real Time, TB - Turn Based) with mode-specific ability systems
- Create a reusable character framework with extensible attributes and abilities

## Project Scope
- **Module Name**: WolfCore
- **Engine Version**: Unreal Engine 5.6
- **Platform**: Windows (DX12, DX11)
- **Language**: C++ with Blueprint integration
- **Repository**: https://github.com/MishaSen/WolfCore

## Key Features
- Gameplay Ability System integration with custom ASC and AttributeSet
- Temporal prediction system (Presage) for combat simulation
- Snapshot-based state management for rollback/undo capabilities
- Multi-mode combat system (Out of Combat, Real Time, Turn Based)
- Enhanced Input system integration
- AI Module and GamePlay State Tree plugins

## Dependencies
- Engine (Core Unreal Engine modules)
- AIModule
- GameplayAbilities
- EnhancedInput
- ModelingToolsEditorMode (Editor only)
- GameplayStateTree