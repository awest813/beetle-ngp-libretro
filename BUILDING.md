# Building Beetle NGP for Dreamcast

## Quick Start

### Prerequisites

Install **KallistiOS** (KOS) with the Dreamcast toolchain. Three options:

| Platform | Recommended | Instructions |
|----------|-------------|-------------|
| **Windows** | [DreamSDK R3+](https://dreamsdk.org) | Installer bundles KOS, GCC/SH4 toolchain, dc-tool, cdi4dc |
| **macOS / Linux** | [KallistiOS from source](https://github.com/KallistiOS/KallistiOS) | Build `dc-chain` cross-compiler, then KOS |
| **Docker** | [dreamcast-toolchain](https://github.com/KallistiOS/KallistiOS-docker) | Pre-built container with all deps |

Verify your setup:
```bash
source $KOS_BASE/environ.sh
kos-cc --version          # sh-elf-gcc (GCC) 9.x / 13.x
scramble --help            # or: $KOS_BASE/utils/scramble/scramble
mkisofs --version          # system package
```

### Build the Standalone Launcher

```bash
cd dreamcast
make              # → beetlengp.elf
```

## Image Formats

### `make cdi` — Self-boot CD-R Image

```bash
cd dreamcast
make cdi          # → beetlengp.cdi
```

Burns to CD-R with DiscJuggler or ImgBurn. Requires `cdi4dc` (included in DreamSDK). Falls back to `.iso` if `cdi4dc` is missing.

To bundle ROMs on the disc:
```bash
make cdi ROMS_DIR=/path/to/roms
```

To embed a single auto-boot ROM:
```bash
make cdi BOOT_ROM=/path/to/game.ngp
```

### `make sdiso` / `make iso` — SD Card Image

```bash
cd dreamcast
make sdiso        # → beetlengp.iso
```

For use with:
- **GDEMU** / **MODE** / **USB-GDROM** — place `.iso` in numbered folder (`/01/`)
- **SD card adapters** — extract or mount

### `make dist` — All Formats

```bash
cd dreamcast
make dist         # → beetlengp.elf + beetlengp.cdi + beetlengp.iso
```

### `make upload` — Network Upload (BBA)

```bash
cd dreamcast
make upload       # sends .elf to Dreamcast via dc-tool-ip
```

## RetroArch Integration

Build RetroArch with Beetle NGP statically linked:

```bash
source $KOS_BASE/environ.sh
cd retroarch
./build-retroarch.sh
```

This produces `../RetroArch/retroarch_dreamcast.elf`.

### Deploy to SD Card

```
/sd/retroarch/
├── retroarch_dreamcast.elf
├── retroarch.cfg          ← copy from retroarch/retroarch.cfg
├── savefiles/
├── savestates/
├── screenshots/
└── roms/
    └── your-game.ngp
```

### RetroArch Controls

| Button | Action |
|--------|--------|
| D-Pad / A / B | NGP controls |
| Start | NGP Option |
| Start + A | Open RetroArch menu |
| Start + X | Load state |
| Start + Y | Save state |

## Standalone Launcher

Copy to SD card and launch with a homebrew loader or via DreamShell:

```
/sd/beetlengp.elf
/sd/ngp/beetlengp.cfg     ← created on first run
/sd/ngp/*.flash            ← battery saves
/sd/ngp/*.state            ← save states
/sd/roms/*.ngp             ← your game files
```

Without a ROM argument, the launcher presents a main menu with ROM browser, settings, and last-game resume.

### Standalone Hotkeys

| Combo | Action |
|-------|--------|
| Start + Y | Save state |
| Start + X | Load state |
| Start + L trigger | Save battery |
| Start + R trigger | Load battery |
| Start + A | Open settings |
| Start + B | Exit |
| Start (tap) | Pause menu |

### Settings (`/sd/ngp/beetlengp.cfg`)

| Key | Values | Default |
|-----|--------|---------|
| `volume` | 0–255 | 200 |
| `scale` | 1–4 | 3 |
| `video` | 0=Auto, 1=VGA, 2=TV | 0 |
| `renderer` | 0=Software, 1=PVR | 0 |
| `vsync` | 0=Off, 1=On | 1 |
| `audio` | 0=Mute, 1=On | 1 |
| `auto_load_state` | 0=Off, 1=On | 0 |
| `vmu_lcd` | 0=Off, 1=On | 1 |
| `vmu_save` | 0=Off, 1=On | 0 |
| `save_dir` | path | `/sd/ngp` |
| `system_dir` | path | `/sd` |
| `last_rom` | path | (empty) |

## Development

### Debug Build

```bash
cd dreamcast
make DEBUG=1       # -O0 -g, no optimizations
```

### VMU Debug Screen

Enable `vmu_lcd=1` to show game preview on the VMU LCD. With `vmu_save=1`, flash saves are mirrored to VMU as `.FLA` files.

### PVR Hardware Renderer

Set `renderer=1` for PowerVR hardware scaling (DMA texture upload, dirty-frame skip). Falls back to software blitter on errors. See `dreamcast/HARDWARE_RENDERING.md` for architecture details.

### Build Flags

| Flag | Default | Description |
|------|---------|-------------|
| `DEBUG=1` | 0 | Disable optimization, enable assertions |
| `USE_COMPUTED_GOTO=1` | auto for Dreamcast | Use the threaded interpreter (see below) |
| `NO_COMPUTED_GOTO=1` | 0 | Force-disable computed-goto (use the original dispatcher) |
| `STATIC_LINKING=1` | auto for Dreamcast | Build as a static archive |
| `FORCE_VFS=1` | auto for static | Include libretro VFS in the static archive |

### Threaded Interpreter (TLCS-900h)

The Dreamcast build uses a **threaded interpreter** for the TLCS-900h CPU core. Instead of a 256-entry function pointer dispatch table (one indirect call per instruction), it uses GCC's `&&label` computed-goto feature to jump directly to a label, saving the function pointer load + jsr + rts overhead per instruction.

The threaded file (`mednafen/ngp/TLCS-900h/TLCS900h_threaded.c`) `#include`s all handler source files plus the original interpreter so everything compiles in one translation unit. This allows GCC to inline across handler boundaries and devirtualize the secondary dispatch.

The threaded build is enabled by `USE_COMPUTED_GOTO=1`, which is set automatically for the Dreamcast target. To verify the build is using the threaded interpreter, check the output of `make`:

```bash
cd dreamcast && make
# If USE_COMPUTED_GOTO is set, TLCS900h_threaded.c is compiled
# and the per-handler .c files are not
```

The threaded interpreter is incompatible with platforms where GCC's `&&label` extension is not available (e.g., MSVC). For those platforms, the build falls back to the original function-pointer dispatcher.

## Troubleshooting

**`scramble: command not found`**
→ Build KOS utilities: `cd $KOS_BASE/utils/scramble && make`

**`mkisofs: command not found`**
→ Install cdrtools: `apt install mkisofs` (Linux) or `brew install cdrtools` (macOS)

**`cdi4dc: command not found`**
→ Install DreamSDK (Windows) or build from [KallistiOS/dc-chain](https://github.com/KallistiOS/KallistiOS)

**VMU saves not visible in BIOS manager**
→ Ensure `vmu_save=1` in config. Saves use the `.FLA` package format with the `BEETLE_NGP` app ID.

**Black screen on boot**
→ Verify cable type (VGA vs composite). Use `video=0` for auto-detect or force with `video=2` for composite/TV.
