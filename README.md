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
