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

Without a ROM argument, the launcher shows a main menu (Load Game / Settings / Exit). Settings are stored at `/sd/ngp/beetlengp.cfg` (volume, scale, video output, **renderer** Software/PVR, vsync, auto-load state, audio on/off, save directory, VMU LCD/save sync). Battery saves (`.flash`) and save states (`.state`) live in the save directory; VMU mirroring writes packaged `.FLA` files to `/vmu/<port>/`. With `renderer=1`, menus and gameplay use the PowerVR path (see `dreamcast/HARDWARE_RENDERING.md`).

## Layout

| Path | Purpose |
|------|---------|
| `overlay/gfx/drivers/dc_gfx.c` | Video driver (software blit or PVR + RGUI overlay) |
| `overlay/audio/drivers/dc_audio.c` | RetroArch wrapper around shared `dreamcast/dc_audio.c` |
| `dreamcast/dc_video.c` | Shared VGA/TV mode selection and blitter (copied at build time) |
| `dreamcast/dc_vmu.c` | VMU LCD status and optional flash mirroring (launcher) |
| `dreamcast/HARDWARE_RENDERING.md` | PowerVR hardware rendering roadmap |
| `dreamcast/dc_audio.c` | Shared KOS `snd_stream` ring buffer (copied at build time) |
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
| Start + A | Open RetroArch menu (or in-game settings in standalone launcher) |
| Start + B | Exit (standalone launcher only) |
| Start + X | Load save state (standalone launcher) |
| Start + Y | Save save state (standalone launcher) |
| Start + L trigger | Save battery (`.flash`) + optional VMU sync |
| Start + R trigger | Reload battery from `.flash` |
| DC B | NGP A button |
| DC A | NGP B button |
| DC Start | NGP Option button |

## Notes

- Video uses a shared `dreamcast/dc_video.c` backend with optional **PVR hardware rendering** (`dreamcast/dc_pvr.c`, setting `renderer=1`). Cable type is detected via `vid_check_cable()`; **Auto** picks the best mode for the cable and scale factor. Settings allow forcing VGA/TV output and Software/PVR renderer.
- Audio uses a shared `dreamcast/dc_audio.c` backend with software volume and mute support. The KOS stream callback expects **byte counts** (not sample frames); output is locked to **44100 Hz** stereo to match Beetle NGP.
- RetroArch reads volume/mute/video from the same `beetlengp.cfg` as the standalone launcher when drivers start.
- Upstream RetroArch has no official Dreamcast port; this overlay lives in beetle-ngp-libretro until drivers can move upstream.
- The static core archive must be named `libretro_dreamcast.a` in the RetroArch build directory.
- RGB555 output is used to match the Dreamcast framebuffer (`PM_RGB555`).
