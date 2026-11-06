# The `ouro` branch

kanjitalk755/macemu with a few changes from other forks, for running
classic Mac games under scripts (the Ouro ports). Everything new is off
unless an environment variable switches it on.

| Change | From | Switch |
|---|---|---|
| arm64 test-and-set and release unlock in SheepShaver's `sysdeps.h` (without it `spin_lock` does nothing and the PowerPC emulation can end by itself) | e-z-g `2abc99f0` | always |
| the same for Basilisk II's `sysdeps.h` (idle and VOSF locks) | ours | always |
| extfs `GetFileInfo` wrote 28 bytes past the 80-byte `HFileParam` | RandoOnSteam `9bc0c66f` | always |
| SDL offscreen driver works (headless runs), failure messages in video setup | e-z-g `0456d8c6` | `SDL_VIDEODRIVER=offscreen` |
| screen dumps as BMP | e-z-g `0456d8c6`, `7666ad89`, `9bb3102d` | `SS_DUMP=path`, `SS_DUMP_MS=ms` |
| mouse and keys from a FIFO (`move X Y`, `down`, `up`, `key N`, `keydown N`, `keyup N`) | e-z-g `0456d8c6` | `SS_INPUT=fifo` |
| video driver trace | e-z-g `7666ad89` | `SS_TRACE_VIDEO=1` |
| the Mac's sound output to a raw file (the host hears silence), one sample rate offered | e-z-g `19c09370` | `SS_SOUND_FILE=path`, `SS_SOUND_RATE=22050` |
| emulator monitor: TCP on 127.0.0.1, `click`, `key tap`, `type`, `shot x.png`, `status`, `quit`, ... (`docs/emumonitor.md`, client `tools/emumonitor.py`) | RandoOnSteam (`emulator_monitor.h`, std::atomic instead of its `atomic.h`) | `EMULATOR_MONITOR_PORT=19840` |
| `SetDateTime` succeeds; Basilisk's clock chip reads the Mac's clock | siddhartha77 `7f367cf8`, `ea205721` | always |
| the Mac clock counts on from its own value instead of copying the host's each second | siddhartha77 (Windows), ours (Unix) | `MACEMU_GUEST_CLOCK=1` |
| S.C.Out frame counter and audio component calls on stderr | ours | `SCOUT_FPS=1` |

Left out: e-z-g's `[present]` lines (printed every ten seconds without
a switch), Android, gfxaccel, JITs.

The emulator monitor's `ping` names the emulator; a second emulator on
the same port runs without a monitor. Monitor and `SS_INPUT` both go to
the emulated ADB mouse and keyboard, never to the host's.
