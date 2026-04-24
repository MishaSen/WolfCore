#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "WolfCore/WolfCore.h"

// ============================================================================================================================
// Debug Configuration Constants
// ============================================================================================================================

/** Boolean flag controlling whether debug logging is enabled for the Wolf framework. Set to 1 by default, disabled in shipping builds. */
#define WOLF_DEBUG_ENABLED 1

#if UE_BUILD_SHIPPING
	#undef  WOLF_DEBUG_ENABLED
	#define WOLF_DEBUG_ENABLED 0
#endif

// ============================================================================================================================
// Debug Logging Macros
// ============================================================================================================================

#if WOLF_DEBUG_ENABLED

/** Formats a debug message with function name, line number, and custom format string for Wolf framework logging. */
#define WOLF_FORMAT_MESSAGE(Format, ...) \
	FString::Printf(TEXT("[Wolf][%s:%d] " Format), TEXT(__FUNCTION__), __LINE__, ##__VA_ARGS__)

/** Logs a formatted message to the Wolf log with specified verbosity level. Only active when debug is enabled. */
/**
 * @param Verbosity The UE log verbosity level (e.g., Log, Warning, Error).
 * @param Format The format string containing placeholders for variadic arguments.
 */
#define WOLF_LOG(Verbosity, Format, ...) \
	do { \
		const FString Message = WOLF_FORMAT_MESSAGE(Format, ##__VA_ARGS__); \
		UE_LOG(LogWolf, Verbosity, TEXT("%s"), *Message); \
	} while (0)

/** Logs an informational message to both the console and on-screen debug display in green color for 3 seconds. */
#define WOLF_INFO(Format, ...) \
	do { \
		const FString Message = WOLF_FORMAT_MESSAGE(Format, ##__VA_ARGS__); \
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, Message); \
		UE_LOG(LogWolf, Log, TEXT("%s"), *Message); \
	} while (0)

/** Logs a warning message to both the console and on-screen debug display in orange color for 4 seconds. */
#define WOLF_WARN(Format, ...) \
	do { \
		const FString Message = WOLF_FORMAT_MESSAGE(Format, ##__VA_ARGS__); \
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Orange, Message); \
		UE_LOG(LogWolf, Warning, TEXT("%s"), *Message); \
	} while (0)

/** Logs an error message to both the console and on-screen debug display in red color for 5 seconds. */
#define WOLF_ERROR(Format, ...) \
	do { \
		const FString Message = WOLF_FORMAT_MESSAGE(Format, ##__VA_ARGS__); \
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Message); \
		UE_LOG(LogWolf, Error, TEXT("%s"), *Message); \
	} while (0)

#else

/** No-op macro for logging when debug is disabled. All arguments are ignored at compile time. */
#define WOLF_LOG(Verbosity, Format, ...)
/** No-op macro for informational messages when debug is disabled. */
#define WOLF_INFO(Format, ...)
/** No-op macro for warning messages when debug is disabled. */
#define WOLF_WARN(Format, ...)
/** No-op macro for error messages when debug is disabled. */
#define WOLF_ERROR(Format, ...)

#endif