#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "WolfCore/WolfCore.h"

#define WOLF_DEBUG_ENABLED 1

#if UE_BUILD_SHIPPING
	#undef  WOLF_DEBUG_ENABLED
	#define WOLF_DEBUG_ENABLED 0
#endif

#if WOLF_DEBUG_ENABLED

#define WOLF_FORMAT_MESSAGE(Format, ...) \
	FString::Printf(TEXT("[Wolf][%s:%d] " Format), TEXT(__FUNCTION__), __LINE__, ##__VA_ARGS__)

#define WOLF_LOG(Verbosity, Format, ...) \
	do { \
		const FString Message = WOLF_FORMAT_MESSAGE(Format, ##__VA_ARGS__); \
		UE_LOG(LogWolf, Verbosity, TEXT("%s"), *Message); \
	} while (0)

#define WOLF_INFO(Format, ...) \
	do { \
		const FString Message = WOLF_FORMAT_MESSAGE(Format, ##__VA_ARGS__); \
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, Message); \
		UE_LOG(LogWolf, Log, TEXT("%s"), *Message); \
	} while (0)

#define WOLF_WARN(Format, ...) \
	do { \
		const FString Message = WOLF_FORMAT_MESSAGE(Format, ##__VA_ARGS__); \
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Orange, Message); \
		UE_LOG(LogWolf, Warning, TEXT("%s"), *Message); \
	} while (0)

#define WOLF_ERROR(Format, ...) \
	do { \
		const FString Message = WOLF_FORMAT_MESSAGE(Format, ##__VA_ARGS__); \
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Message); \
		UE_LOG(LogWolf, Error, TEXT("%s"), *Message); \
	} while (0)

#else

#define WOLF_LOG(Verbosity, Format, ...)
#define WOLF_INFO(Format, ...)
#define WOLF_WARN(Format, ...)
#define WOLF_ERROR(Format, ...)

#endif