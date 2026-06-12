# Hardware Rendering Plan (PowerVR / PVR)

This document outlines how to move Beetle NGP Dreamcast video from the current **software RGB555 VRAM blit** (`dc_video.c`) to **PowerVR hardware presentation**, while keeping menus and settings on the existing path until the transition is complete.

## Current state

| Component | Implementation |
|-----------|----------------|
| Display setup | `vid_set_mode()` — cable/scale-aware VGA/TV modes |
| Framebuffer | Host buffer → integer upscale → `memcpy(vram_s, …)` |
| Pixel format | RGB555 / ARGB1555 (15-bit), matches core output |
| Scale | 2×–4× in software (`dc_video_blitter_rgb555`) |
| RetroArch | `dc_gfx.c` uses the same `dc_video` blitter |

**Bottleneck:** Every frame copies a full 640×480 (or 800×608) buffer on the SH-4 CPU. At 60 FPS this is acceptable for NGP but leaves no headroom for filters, overlays, or second-pass UI.

## Goals

1. Upload the 160×152 core framebuffer to a **PVR texture** once per frame (or less with dirty rects).
2. Draw a **single scaled quad** via the TA so scaling happens in hardware.
3. Keep **cable detection and mode selection** in `dc_video` (unchanged).
4. Allow a runtime toggle: `video_renderer = software | pvr` (future setting).

## Architecture

```
retro_run()
  └─ video_refresh(160×152 RGB555)
       └─ dc_video_present_frame()
            ├─ [software] dc_video_blitter_rgb555 + memcpy(vram_s)
            └─ [pvr]      pvr_txr_load_ex → pvr_list_begin → poly → pvr_finish
```

Launcher menus use `dc_ui.c` (shared RGB555 buffer + `dc_pvr_present_ui` when PVR is active). Notifications use a PVR TR overlay during gameplay. VMU LCD stays on Maple `vmufb` (separate from the main PVR path).

## Implementation status

| Phase | Status |
|-------|--------|
| 1 — PVR init + texture upload | **Done** (`dreamcast/dc_pvr.c`) |
| 2 — Scaled hardware quad | **Done** (scale via polygon size) |
| 3 — `dc_video` integration + setting | **Done** (`renderer=0\|1` in cfg, launcher) |
| 4 — Optimizations | **Done** (async DMA, double-buffer, dirty skip, frame dup) |
| 5 — PVR UI | **Done** (launcher menus via `dc_ui` + `dc_pvr_present_ui`) |

Launcher setting: **Renderer → Software / PVR**. Menus temporarily shut down PVR and use `vram_s` directly.

## Phase 1 — PVR init and texture upload

**Files:** `dreamcast/dc_pvr.c`, `dreamcast/dc_pvr.h`

- Call `pvr_init()` with a minimal bin configuration (opaque polygons, no Z).
- Allocate PVR texture RAM for **160×152 ARGB1555** (core native size).
- Upload each frame with `pvr_txr_load_ex()`:
  - Format: `PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED`
  - Width 160 is not a power of two → use `PVR_TXRFMT_X32_STRIDE` and `pvr_txr_set_stride(160)` (KOS supports widths that are multiples of 32 up to 992).
- Verify one static full-screen quad at 1× scale on VGA before enabling integer upscale.

**Exit criteria:** Static test pattern or live NGP frame visible via PVR at 1× with no software `memcpy` to `vram_s`.

## Phase 2 — Scaled quad renderer

- Replace CPU nested-loop scale with **polygon UV/size scaling**:
  - Destination size = `160 * scale` × `152 * scale`, centered.
  - Filtering: point sampling (nearest) to preserve crisp pixels; optional `PVR_FILTER_NONE`.
- Handle mode sizes (640×480, 800×608, 320×240) by adjusting polygon position/size only.
- VSync: tie to `pvr_finish()` / `pvr_wait_ready()` instead of `vid_waitvbl()` when PVR owns the frame.

**Exit criteria:** 2×/3×/4× match software output on VGA and composite spot-check.

## Phase 3 — Integration behind `dc_video`

- Add `dc_video_renderer_t` enum and setting `video_renderer` in `beetlengp.cfg`.
- Refactor `dc_video_blitter_present()`:

```c
void dc_video_blitter_present(dc_video_blitter_t *b, bool vsync)
{
   if (active_renderer == DC_VIDEO_RENDERER_PVR)
      dc_pvr_present(b, vsync);
   else
      dc_video_present(b->buffer, vsync);
}
```

- Update `retroarch/overlay/gfx/drivers/dc_gfx.c` to use the same entry point.
- Menus: `dc_ui` renders to a CPU buffer and presents via PVR or `vram_s`; PVR stays initialized (no shutdown on menu open).

**Exit criteria:** Launcher and RetroArch both run with `video_renderer=pvr` on hardware.

## Phase 4 — Optimizations

**Status:** Implemented in `dc_pvr.c`.

| Optimization | Benefit |
|--------------|---------|
| `pvr_txr_load_dma` (non-blocking) | DMA upload overlaps PVR scene render |
| Double-buffered PVR texture | Ping-pong staging + VRAM textures |
| Dirty frame skip | Skip DMA when staging matches last frame |
| Frame dup (`src == NULL`) | Skip upload; redraw last texture + overlay |
| PVR notification overlay | Toasts on translucent TR poly (`dc_notify.c`) |
| 256×256 padded texture | Stride mode for 160×152 game framebuffer |
| RGB565 PVR format | If core ever runs 16-bit on DC (currently 15-bit) |

Launcher and RetroArch pass `NULL` to `video_refresh` / `dc_gfx_frame` on frame dup;
`dc_video_present_rgb555(NULL, …)` re-presents the last uploaded texture.

## Technical constraints

- **Texture size:** 160×152 is awkward for twiddled POW2 paths; prefer **stride mode** or pad to **256×256** with unused texels.
- **PVR + direct VRAM:** Do not mix `vram_s` writes and PVR output in the same frame without explicit sync.
- **Memory:** Budget ~200 KB for textures + polygon lists; well within DC limits for a single layer.
- **RGB555 byte order:** Confirm ARGB1555 packing matches `pvr_txr_load_ex` expectations (same as current `PM_RGB555`).

## Risks and mitigations

| Risk | Mitigation |
|------|------------|
| PVR init breaks existing menus | Software fallback; menu forces software renderer |
| Stride textures misaligned | Unit test with checkerboard; fall back to 256×256 pad |
| Interlaced TV modes | Test Phase 2 on `DM_640x480_NTSC_IL`; may need field flag |
| RetroArch RGUI on PVR | RGUI may need software blit overlay until Phase 5 |

## Phase 5 — PVR UI (launcher + pause)

**Status:** Implemented in `dc_ui.c` / `dc_pvr_present_ui()`.

- Shared UI toolkit: headers, footers, panels, scrollable lists, scrollbars (`dc_ui_*`).
- Launcher menus (main, ROM browser, settings) and in-game pause overlay use the same theme.
- Settings list scrolls on 320×240; status panel shows VMU/cable/save info.
- `dc_ui_present` uploads the full screen and draws a 1:1 PVR quad when `renderer=pvr`.
- Toasts (`dc_notify`) reuse `DC_UI_COLOR_ACCENT` for a consistent look.
- `dc_video_menu_begin/end` use a depth counter; PVR is no longer torn down for menus.
- RetroArch RGUI uses the video poke interface (`set_texture_frame` / `set_texture_enable`) to composite a translucent PVR menu overlay in `dc_gfx.c`.

### Controls

| Context | Input |
|---------|--------|
| Boot | Splash screen; press any button to skip |
| Launcher | Continue last game (if available), D-Pad move (hold to repeat), A select |
| ROM browser | Sorted A–Z, remembers last game, S/B badges for state/battery |
| In-game pause | Start opens menu; confirm load/quit; Start+Y/X/L/R quick shortcuts |
| Settings | Per-item hints, Left/Right change values, save acknowledgment flash |

VMU preview remains on Maple LCD (`vmufb`). VMU flash mirroring uses `vmu_pkg_build` /
`vmu_pkg_parse` for BIOS-visible `.FLA` files, with periodic Maple rescan and a
built-in 32×32 icon.

## Suggested implementation order

1. `dc_pvr_init` / `dc_pvr_shutdown` + hello-world quad
2. Live NGP texture upload at 1×
3. Integer scale quads + `dc_video` integration toggle
4. RetroArch `dc_gfx` parity
5. DMA / double-buffer optimizations

## References

- KOS: `dc/pvr/pvr.h`, `dc/pvr/pvr_txr.h`, `examples/dreamcast/pvr/`
- Current software path: `dreamcast/dc_video.c`
- Cable/mode logic: `dc_video_init_for_scale()`
