#pragma once

#define ALTSOUND_VERSION_MAJOR 0 // X Digits
#define ALTSOUND_VERSION_MINOR 1 // Max 2 Digits
#define ALTSOUND_VERSION_PATCH 0 // Max 2 Digits

#define _ALTSOUND_STR(x) #x
#define ALTSOUND_STR(x) _ALTSOUND_STR(x)

#define ALTSOUND_VERSION ALTSOUND_STR(ALTSOUND_VERSION_MAJOR) "." ALTSOUND_STR(ALTSOUND_VERSION_MINOR) "." ALTSOUND_STR(ALTSOUND_VERSION_PATCH)
#define ALTSOUND_MINOR_VERSION ALTSOUND_STR(ALTSOUND_VERSION_MAJOR) "." ALTSOUND_STR(ALTSOUND_VERSION_MINOR)

#ifdef _MSC_VER
#define ALTSOUNDAPI extern "C" __declspec(dllexport)
#define ALTSOUNDCALLBACK __stdcall
#else
#define ALTSOUNDAPI extern "C" __attribute__((visibility("default")))
#define ALTSOUNDCALLBACK
#endif

#include <cstdint>
#include <string>

using std::string;

typedef enum : uint64_t {
	ALTSOUND_HARDWARE_GEN_NONE = 0x0000000000000,
	ALTSOUND_HARDWARE_GEN_WPCALPHA_1 = 0x0000000000001,  // Alpha-numeric display S11 sound, Dr Dude 10/90
	ALTSOUND_HARDWARE_GEN_WPCALPHA_2 = 0x0000000000002,  // Alpha-numeric display,  - The Machine BOP 4/91
	ALTSOUND_HARDWARE_GEN_WPCDMD = 0x0000000000004,      // Dot Matrix Display, Terminator 2 7/91 - Party Zone 10/91
	ALTSOUND_HARDWARE_GEN_WPCFLIPTRON = 0x0000000000008, // Fliptronic flippers, Addams Family 2/92 - Twilight Zone 5/93
	ALTSOUND_HARDWARE_GEN_WPCDCS = 0x0000000000010,      // DCS Sound system, Indiana Jones 10/93 - Popeye 3/94
	ALTSOUND_HARDWARE_GEN_WPCSECURITY = 0x0000000000020, // Security chip, World Cup Soccer 3/94 - Jackbot 10/95
	ALTSOUND_HARDWARE_GEN_WPC95DCS = 0x0000000000040,    // Hybrid WPC95 driver + DCS sound, Who Dunnit
	ALTSOUND_HARDWARE_GEN_WPC95 = 0x0000000000080,       // Integrated boards, Congo 3/96 - Cactus Canyon 2/99
	ALTSOUND_HARDWARE_GEN_S11 = 0x0000080000000,         // No external sound board
	ALTSOUND_HARDWARE_GEN_S11X = 0x0000000000100,        // S11C sound board
	ALTSOUND_HARDWARE_GEN_S11B2 = 0x0000000000200,       // Jokerz! sound board
	ALTSOUND_HARDWARE_GEN_S11C = 0x0000000000400,        // No CPU board sound
	ALTSOUND_HARDWARE_GEN_DE = 0x0000000001000,          // DE AlphaSeg
	ALTSOUND_HARDWARE_GEN_DEDMD16 = 0x0000000002000,     // DE 128x16
	ALTSOUND_HARDWARE_GEN_DEDMD32 = 0x0000000004000,     // DE 128x32
	ALTSOUND_HARDWARE_GEN_DEDMD64 = 0x0000000008000,     // DE 192x64
	ALTSOUND_HARDWARE_GEN_GTS80 = 0x0000200000000,       // GTS80 / Gottlieb System 80
	//ALTSOUND_HARDWARE_GEN_GTS80A = ALTSOUND_HARDWARE_GEN_GTS80,
	ALTSOUND_HARDWARE_GEN_WS = 0x0004000000000,          // Whitestar
	ALTSOUND_HARDWARE_GEN_WS_1 = 0x0008000000000,        // Whitestar with extra RAM
	ALTSOUND_HARDWARE_GEN_WS_2 = 0x0010000000000,        // Whitestar with extra DMD
} ALTSOUND_HARDWARE_GEN;

typedef enum {
	ALTSOUND_LOG_LEVEL_NONE = 0,
	ALTSOUND_LOG_LEVEL_INFO,
	ALTSOUND_LOG_LEVEL_ERROR,
	ALTSOUND_LOG_LEVEL_WARNING,
	ALTSOUND_LOG_LEVEL_DEBUG,
	ALTSOUND_LOG_LEVEL_UNDEFINED,
} ALTSOUND_LOG_LEVEL;

ALTSOUNDAPI void AltsoundSetLogger(const string& logPath, ALTSOUND_LOG_LEVEL logLevel, bool console);
ALTSOUNDAPI bool AltsoundInit(const string& pinmamePath, const string& gameName);
ALTSOUNDAPI void AltsoundSetHardwareGen(ALTSOUND_HARDWARE_GEN hardwareGen);
ALTSOUNDAPI bool AltsoundProcessCommand(const unsigned int cmd, int attenuation);
ALTSOUNDAPI void AltsoundPause(bool pause);
ALTSOUNDAPI void AltsoundShutdown();