# libaltsound

A cross platform compilation of [altsound](https://github.com/vpinball/pinmame/tree/master/src/wpc/altsound) that is baked directly into [VPinMAME](https://github.com/vpinball/pinmame).

Suitable for use by third party applications that use [libpinmame](https://github.com/vpinball/pinmame/tree/master/src/libpinmame), such as [Visual Pinball Standalone](https://github.com/vpinball/vpinball/tree/standalone).

## Audio Engine

This library uses miniaudio for cross-platform audio processing. The library does not directly output audio to hardware. Instead, it provides mixed audio data through a callback function that you must implement. You are responsible for routing this audio data to your preferred audio output system.

## Usage:

### Basic Setup

```c++
#include "altsound.h"

// Audio callback function - you must implement this
void audio_callback(const float* samples, size_t frameCount, uint32_t sampleRate, uint32_t channels, void* userData)
{
}

void MyController::OnSoundCommand(int boardNo, int cmd)
{
    AltSoundProcessCommand(cmd, 0);
}

// Initialize logger
AltSoundSetLogger("/Users/jmillard/altsound", ALTSOUND_LOG_LEVEL_INFO, false);

// Initialize altsound with audio configuration
AltSoundInit("/Users/jmillard/.pinmame", "gnr_300", 44100, 2, 256);
AltSoundSetHardwareGen(ALTSOUND_HARDWARE_GEN_DEDMD32);

// Set up audio callback
AltSoundSetAudioCallback(audio_callback, nullptr);

// Process sound commands
AltSoundProcessCommand(cmd, 0);

// Pause/resume playback
AltSoundPause(true);
AltSoundPause(false);

// Cleanup
AltSoundShutdown();
```

## Building:

#### Windows (x64)

```shell
cmake -G "Visual Studio 17 2022" -DPLATFORM=win -DARCH=x64 -B build
cmake --build build --config Release
```

#### Windows (x86)

```shell
cmake -G "Visual Studio 17 2022" -A Win32 -DPLATFORM=win -DARCH=x86 -B build
cmake --build build --config Release
```

#### Linux (x64)
```shell
cmake -DPLATFORM=linux -DARCH=x64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### Linux (aarch64)
```shell
cmake -DPLATFORM=linux -DARCH=aarch64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### MacOS (arm64)
```shell
cmake -DPLATFORM=macos -DARCH=arm64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### MacOS (x64)
```shell
cmake -DPLATFORM=macos -DARCH=x64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### iOS (arm64)
```shell
cmake -DPLATFORM=ios -DARCH=arm64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### iOS Simulator (arm64)
```shell
cmake -DPLATFORM=ios-simulator -DARCH=arm64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### tvOS (arm64)
```shell
cmake -DPLATFORM=tvos -DARCH=arm64 -DBUILD_SHARED=OFF -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### Android (arm64-v8a)
```shell
cmake -DPLATFORM=android -DARCH=arm64-v8a -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```
