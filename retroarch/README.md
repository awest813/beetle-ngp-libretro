# RetroArch static integration (Dreamcast)

Optional build path that links **Beetle NeoGeo Pocket** into [RetroArch](https://github.com/libretro/RetroArch) as a static core, using KallistiOS platform drivers.

## Prerequisites

- DreamSDK / KallistiOS toolchain (`kos-cc`, `kos-c++`)
- `patch` utility
- Network access to clone RetroArch (first build only)

## Quick build

```bash
source $KOS_BASE/environ.sh
cd /path/to/beetle-ngp-libretro
./retroarch/build-retroarch.sh
```

This will:

1. Build `libretro_dreamcast.a` from the core Makefile (`platform=dreamcast`)
2. Clone `../RetroArch` if it does not exist
3. Copy `retroarch/overlay/` drivers into the RetroArch tree
4. Apply patches from `retroarch/patches/`
5. Produce `../RetroArch/retroarch_dreamcast.elf`

## Deploy to hardware

Copy to your SD card:

```
/sd/retroarch/retroarch_dreamcast.elf
/sd/retroarch/retroarch.cfg          # from retroarch/retroarch.cfg
/sd/retroarch/savefiles/
/sd/retroarch/savestates/
```

Launch with a ROM path argument, or use the RGUI content browser (`Start+A` opens the menu).

## Standalone launcher

The simpler `dreamcast/beetlengp.elf` launcher (no RetroArch) remains available:

```bash
source $KOS_BASE/environ.sh
cd dreamcast
make
```

## Layout

| Path | Purpose |
|------|---------|
| `overlay/gfx/drivers/dc_gfx.c` | RGB555 VRAM blit video driver |
| `overlay/audio/drivers/dc_audio.c` | `snd_stream` audio driver |
| `overlay/input/drivers_joypad/dc_joypad.c` | Maple controller joypad |
| `overlay/input/drivers/dc_input.c` | Minimal input driver |
| `overlay/frontend/drivers/platform_dreamcast.c` | Paths and drive list |
| `patches/0001-dreamcast-platform.patch` | RetroArch registration hooks |
| `Makefile.dreamcast` | Console Makefile (pattern from `Makefile.ngc`) |
| `build-retroarch.sh` | End-to-end build helper |

## Controls

| Button | Action |
|--------|--------|
| D-Pad / A / B / X / Y | Standard NGP mapping (configure in RGUI) |
| Start + A | Open RetroArch menu |
| Start + B | Exit (standalone launcher only) |

## Notes

- Upstream RetroArch has no official Dreamcast port; this overlay lives in beetle-ngp-libretro until drivers can move upstream.
- The static core archive must be named `libretro_dreamcast.a` in the RetroArch build directory.
- RGB555 output is used to match the Dreamcast framebuffer (`PM_RGB555`).
