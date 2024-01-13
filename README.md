# libaltsound

A cross platform compilation of [altsound](https://github.com/vpinball/pinmame/tree/master/src/wpc/altsound) that is baked directly into [VPinMAME](https://github.com/vpinball/pinmame).

Suitable for use by third party applications that use [libpinmame](https://github.com/vpinball/pinmame/tree/master/src/libpinmame), such as [Visual Pinball Standalone](https://github.com/vpinball/vpinball/tree/standalone).

## Usage:

```c++
#include "altsound.h"
.
.
void MyController::OnSoundCommand(int boardNo, int cmd)
{
   AltsoundProcessCommand(cmd, 0);
}
.
.
AltsoundSetLogger("/Users/jmillard/altsound", ALTSOUND_LOG_LEVEL_INFO, false);
AltsoundInit("/Users/jmillard/.pinmame", "gnr_300");
AltsoundSetHardwareGen(ALTSOUND_HARDWARE_GEN_DEDMD32);
.
.
.
AltsoundPause(true);
.
.
.
AltsoundShutdown();
```

## Building:

#### Windows (x64)

```shell
platforms/win/x64/external.sh
cmake -G "Visual Studio 17 2022" -DPLATFORM=win -DARCH=x64 -B build
cmake --build build --config Release
```

#### Windows (x86)

```shell
platforms/win/x86/external.sh
cmake -G "Visual Studio 17 2022" -A Win32 -DPLATFORM=win -DARCH=x86 -B build
cmake --build build --config Release
```

#### Linux (x64)
```shell
platforms/linux/x64/external.sh
cmake -DPLATFORM=linux -DARCH=x64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### Linux (aarch64)
```shell
platforms/linux/aarch64/external.sh
cmake -DPLATFORM=linux -DARCH=aarch64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### MacOS (arm64)
```shell
platforms/macos/arm64/external.sh
cmake -DPLATFORM=macos -DARCH=arm64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### MacOS (x64)
```shell
platforms/macos/x64/external.sh
cmake -DPLATFORM=macos -DARCH=x64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### iOS (arm64)
```shell
platforms/ios/arm64/external.sh
cmake -DPLATFORM=ios -DARCH=arm64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### tvOS (arm64)
```shell
platforms/tvos/arm64/external.sh
cmake -DPLATFORM=tvos -DARCH=arm64 -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

#### Android (arm64-v8a)
```shell
platforms/android/arm64-v8a/external.sh
cmake -DPLATFORM=android -DARCH=arm64-v8a -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```
