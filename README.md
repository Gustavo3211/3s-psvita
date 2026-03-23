# Street Fighter III: 3rd Strike - PSP Port

A PSP port of Street Fighter III: 3rd Strike, based on the PS2/PC decompilation.

## Features

### Rendering
- Native CPS3 resolution (384x224) centered on PSP screen (480x272)
- Per-vertex offset scaling with scissor clipping for clean pixel rendering
- No sub-pixel seams or texture atlas bleeding
- Shadow transparency via GU_TFX_MODULATE

### Audio - Complete Pipeline
- **BGM**: CRI ADX ADPCM decoder with 48kHz to 44.1kHz resampling
- **SFX**: PS2 SPU2 emulator (48 voices, VAG/ADPCM, ADSR envelopes)
- **Character voices**: Per-character BD sound banks loaded from AFS
- **VS screen jingle**: In-memory ADX playback (ADX_StartMem)
- **Seamless BGM**: Gapless segment transitions with preload system
- **Capcom Sound Engine (CSE)**: Full PHD/TSB bank system for SE dispatch

### I/O
- AFS archive reader with background I/O thread
- Async file reads for asset loading (LDREQ queue)
- Sync reads for audio (gapless transitions)
- Single AFS file handle design

### Performance
- 333MHz CPU clock
- SPU emulator: MAX_ACTIVE_VOICES=16 cap, linear interpolation, unrolled ADPCM decode
- -O3 -ffast-math compiler optimization
- Graceful error recovery (no freeze traps)

## Building

Requires [pspdev](https://github.com/pspdev/pspdev) toolchain (tested via WSL Debian).

```bash
export PSPDEV=/usr/local/pspdev
export PATH="$PSPDEV/bin:$PATH"
mkdir build_wsl && cd build_wsl
psp-cmake ..
make -j4
```

## Resources

You need `SF33RD.AFS` from the PS2 disc. Place it in a `resources/` folder next to `EBOOT.PBP`:

```
ms0:/PSP/GAME/3rd-strike/
    EBOOT.PBP
    ICON0.PNG          (optional, 144x80 RGBA PNG)
    resources/
        SF33RD.AFS
```

## Controls

- Start+Select: Soft reset (returns to title screen)
- Press Start at boot: Disable backgrounds (debug)

## Known Issues

- Seamless BGM segment transitions have a minor audible click on some tracks
- White noise burst possible on SPU voices with invalid start addresses (mitigated with address validation)

## TODO / Stubs

### Save System
- `SaveInit()` and `SaveMove()` are stubs — no save/load support yet
- Would need PSP `sceUtilitySavedata*` API integration

### Audio Polish
- `ADX_SetMono()` — mono mode not implemented (PSP always stereo)
- ADX loading is fully synchronous — could benefit from async with double-buffered player state
- Seamless preload system could use a dedicated I/O fd to avoid LDREQ conflicts

### Rendering
- MTRANS error traps converted to early returns — some texture groups may not load in time on first frame
- `ppgPurgeTextureFromVRAM` / `ppgPurgePaletteFromVRAM` — PS2 VRAM ops disabled, PSP equivalent not needed

### General
- `fatal_error()`, `not_implemented()`, `debug_print()` — empty logging functions
- `tarPADDestroy()` — PAD cleanup stub
- `flMemset()` / `flMemcpy()` — manual loops, should use stdlib
