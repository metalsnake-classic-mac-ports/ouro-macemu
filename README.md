# ouro-macemu

Basilisk II and SheepShaver from [kanjitalk755/macemu](https://github.com/kanjitalk755/macemu),
branch `ouro`, with a few changes collected from other forks. It is the
emulator used to study the original games behind the
[Ouro ports](https://github.com/metalsnake-classic-mac-ports) and to drive
them from scripts: start without a window, click, type and take
screenshots through a local socket, record the sound.

Everything new is **off unless an environment variable switches it on**;
only the three bug fixes are always active. Without those variables the
emulators behave like kanjitalk755's. The branch is kept on top of
kanjitalk755's `master` (rebased, not merged); `master` here follows
upstream unchanged.

## What is different from kanjitalk755/macemu

### Bug fixes (always on)

| Change | Why | From |
|---|---|---|
| SheepShaver `sysdeps.h`: a test-and-set and a release-store unlock for arm64 | arm64 had no `testandset`, so `spin_lock` did nothing: the CPU's `spcflags` and `InterruptFlags` were changed from two threads without a lock, and a lost update could end the PowerPC emulation silently in the middle of a game | [e-z-g `2abc99f0`](https://github.com/e-z-g/macemu/commit/2abc99f02eec6d90390c99ed62f15cb03f8b8876) |
| Basilisk II `sysdeps.h`: the same | the same gap; the idle lock (`idlewait true`) and the VOSF lock did not lock on arm64 | this fork |
| extfs `GetFileInfo` / `SetFileInfo` | wrote 28 bytes past the 80-byte `HFileParam` (fields that only exist in the larger `CInfoPBRec`); apps launched from the shared folder did not come to the front | [RandoOnSteam `9bc0c66f`](https://github.com/RandoOnSteam/PocketShaver/commit/9bc0c66f03f2708341b2b03da9166598e205adcd) |

### Scripted and headless runs (switched by environment variables)

| Variable | Effect | From |
|---|---|---|
| `EMULATOR_MONITOR_PORT=19840` | **emulator monitor**: one-line commands over TCP on 127.0.0.1 - `status`, `shot x.png` (from the Mac's frame buffer), `mouse X Y`, `mousedown 0`, `mouseup 0`, `click X Y`, `dblclick`, `drag`, `hold X Y MS`, `key down/up/tap CODE` (ADB codes), `type TEXT`, `power`, `quit`; input goes to the emulated ADB mouse and keyboard, never to the host's. Protocol and commands: [docs/emumonitor.md](docs/emumonitor.md), client [tools/emumonitor.py](tools/emumonitor.py) | RandoOnSteam/PocketShaver ([`f8e67ed8`](https://github.com/RandoOnSteam/PocketShaver/commit/f8e67ed820d486f8317f096b5ed31b2e81c4cc15) and later); here with `std::atomic` instead of that tree's `atomic.h`, started only when the variable is set, SheepShaver answers `ping` as SheepShaver |
| `SDL_VIDEODRIVER=offscreen` | no window at all (SDL's software renderer); combine with `SDL_AUDIODRIVER=dummy` for silence | [e-z-g `0456d8c6`](https://github.com/e-z-g/macemu/commit/0456d8c64100289fa93ef06ef2d02848f63db88c); this fork: works under sdl2-compat (SDL3), which otherwise fails for lack of an EGL library |
| `SS_DUMP=path`, `SS_DUMP_MS=ms` | the screen saved as BMP every `ms` (default 1000) | e-z-g [`0456d8c6`](https://github.com/e-z-g/macemu/commit/0456d8c64100289fa93ef06ef2d02848f63db88c), [`7666ad89`](https://github.com/e-z-g/macemu/commit/7666ad899ab1b52742ef9035ca5d8aa8bc2ddce2), [`9bb3102d`](https://github.com/e-z-g/macemu/commit/9bb3102d3ba318ba719b41d0b912a79305297121) |
| `SS_INPUT=fifo` | mouse and keys from a FIFO: `move X Y`, `down`, `up`, `key N`, `keydown N`, `keyup N` | e-z-g [`0456d8c6`](https://github.com/e-z-g/macemu/commit/0456d8c64100289fa93ef06ef2d02848f63db88c) |
| `SS_TRACE_VIDEO=1` | logs the video driver's control and status calls | e-z-g [`7666ad89`](https://github.com/e-z-g/macemu/commit/7666ad899ab1b52742ef9035ca5d8aa8bc2ddce2) |
| `SS_SOUND_FILE=path`, `SS_SOUND_RATE=22050` | the Mac's sound output written raw to a file in the device's format (16-bit big-endian or 8-bit unsigned), paced by the sound card; the host hears silence. `SS_SOUND_RATE` offers the Mac a single sample rate | e-z-g [`19c09370`](https://github.com/e-z-g/macemu/commit/19c0937064fe2b253b019a9b367f40da1e83606c) |

### The Mac's clock

| Change | Switch | From |
|---|---|---|
| `SetDateTime` succeeds (no clock chip check); Basilisk II's clock chip reads the Mac's own clock once it runs | always | siddhartha77 [`7f367cf8`](https://github.com/siddhartha77/macemu/commit/7f367cf8eb62a3d981bc03012e58eb08531656f8), [`ea205721`](https://github.com/siddhartha77/macemu/commit/ea2057210ccd90dd8867134a74636a7daae3c47a) |
| the Mac clock counts on from its own value instead of being overwritten with the host time every second, so a date set inside the Mac stays | `MACEMU_GUEST_CLOCK=1` (Unix; siddhartha77 changed only the Windows mains, unconditionally) | this fork |

Without `MACEMU_GUEST_CLOCK` the host time is copied every second as
before, so setting a date inside the Mac is accepted but does not last.

### Game-specific

| Change | Switch |
|---|---|
| Basilisk II prints S.C.Out's frame counter, Mac ticks per second and the first 20 audio component calls to stderr | `SCOUT_FPS=1` |

### Left out on purpose

From e-z-g: the Android port and the `[present]` lines printed every ten
seconds without a switch. From the PocketShaver forks (carbjo, sierra760,
RandoOnSteam): Metal graphics acceleration, the arm64 JIT, iOS / Mac
Catalyst, Mac OS X guests - too far from upstream and not needed for 2D
games.

## Building on macOS (Unix build, SDL2 through sdl2-compat, no JIT)

    brew install autoconf automake libtool mpfr gmp sdl2-compat
    export CFLAGS="-O2 -std=gnu89 -Wno-implicit-function-declaration -Wno-int-conversion"
    export CPPFLAGS="-I/opt/homebrew/include" LDFLAGS="-L/opt/homebrew/lib"
    # Basilisk II
    cd BasiliskII/src/Unix && NO_CONFIGURE=1 ./autogen.sh
    ./configure --enable-sdl-video --enable-sdl-audio --without-gtk \
        --disable-jit-compiler --without-mon --without-esd --without-x && make -j8
    # SheepShaver
    make -C SheepShaver links
    cd SheepShaver/src/Unix && NO_CONFIGURE=1 ./autogen.sh
    ./configure --enable-sdl-video --enable-sdl-audio --without-gtk \
        --without-mon --without-esd --without-x --disable-jit && make -j8

The Makefiles have no header dependencies: run `make clean` after
changing a header. `-std=gnu89` is for slirp's K&R C.

## A headless run

    SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy EMULATOR_MONITOR_PORT=19840 \
        BasiliskII/src/Unix/BasiliskII --config my.prefs &
    python3 tools/emumonitor.py status
    python3 tools/emumonitor.py shot /tmp/mac.png
    python3 tools/emumonitor.py click 20 10

Coordinates are the Mac's. The monitor's `quit` ends the emulator without
shutting the Mac down (Mac OS 9 checks its disk at the next start); a
clean shutdown through the Mac (Special > Shut Down, or `power`) has not
been tried with the monitor yet. Checked on macOS
(arm64) with Basilisk II (System 7.5) and SheepShaver (Mac OS 9.0.4).

## Licence

GPL-2.0, like Basilisk II and SheepShaver. The changes taken from other
forks keep their authors in the commit history.

---

*kanjitalk755's README follows.*

#### BasiliskII
```
macOS     x86_64 JIT / arm64 non-JIT
Linux x86 x86_64 JIT / arm64 non-JIT
MinGW x86        JIT
```
#### SheepShaver
```
macOS     x86_64 JIT / arm64 non-JIT
Linux x86 x86_64 JIT / arm64 non-JIT
MinGW x86        JIT
```
### How To Build
These builds need to be installed SDL2.0.14+ framework/library.

https://www.libsdl.org
#### BasiliskII
##### macOS
preparation:

Download gmp-6.2.1.tar.xz from https://gmplib.org.
```
$ cd ~/Downloads
$ tar xf gmp-6.2.1.tar.xz
$ cd gmp-6.2.1
$ ./configure --disable-shared
$ make
$ make check
$ sudo make install
```
Download mpfr-4.2.0.tar.xz from https://www.mpfr.org.
```
$ cd ~/Downloads
$ tar xf mpfr-4.2.0.tar.xz
$ cd mpfr-4.2.0
$ ./configure --disable-shared
$ make
$ make check
$ sudo make install
```
On an Intel Mac, the libraries should be cross-built.  
Change the `configure` command for both GMP and MPFR as follows, and ignore the `make check` command:
```
$ CFLAGS="-arch arm64" CXXFLAGS="$CFLAGS" ./configure -host=aarch64-apple-darwin --disable-shared 
```
(from https://github.com/kanjitalk755/macemu/pull/96)

about changing Deployment Target:  
If you build with an older version of Xcode, you can change Deployment Target to the minimum it supports or 10.7, whichever is greater.

build:
```
$ cd macemu/BasiliskII/src/MacOSX
$ xcodebuild build -project BasiliskII.xcodeproj -configuration Release
```

##### Linux
preparation (arm64 only): Install GMP and MPFR.
```
$ cd macemu/BasiliskII/src/Unix
$ ./autogen.sh
$ make
```
##### MinGW32/MSYS2
preparation:
```
$ pacman -S base-devel mingw-w64-i686-toolchain autoconf automake mingw-w64-i686-SDL2
```
note: MinGW32 dropped GTK2 package.
See msys2/MINGW-packages#24490

build (from a mingw32.exe prompt):
```
$ cd macemu/BasiliskII/src/Windows
$ ../Unix/autogen.sh
$ make
```
#### SheepShaver
##### macOS
about changing Deployment Target: see BasiliskII
```
$ cd macemu/SheepShaver/src/MacOSX
$ xcodebuild build -project SheepShaver_Xcode8.xcodeproj -configuration Release
```

##### Linux
```
$ cd macemu/SheepShaver/src/Unix
$ ./autogen.sh
$ make
```
For Raspberry Pi:
https://github.com/vaccinemedia/macemu

##### MinGW32/MSYS2
preparation: same as BasiliskII  
  
build (from a mingw32.exe prompt):
```
$ cd macemu/SheepShaver
$ make links
$ cd src/Windows
$ ../Unix/autogen.sh
$ make
```
### Recommended key bindings for gnome
https://github.com/kanjitalk755/macemu/blob/master/SheepShaver/doc/Linux/gnome_keybindings.txt

(from https://github.com/kanjitalk755/macemu/issues/59)
