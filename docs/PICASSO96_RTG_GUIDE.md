# Picasso96 / RTG Guide for UAE4ALL2 HD on Nintendo Switch

This guide explains how to enable and use the optional Picasso96/RTG support in UAE4ALL2 HD.

## 1. What Picasso96/RTG is

Picasso96 is the Amiga graphics API used by Workbench, productivity applications, demos, and games that need a chunky framebuffer instead of the normal Amiga bitplane display.

In UAE4ALL2 HD, RTG is provided through an emulated UAEGFX-compatible board with a separate linear VRAM area. The RTG path is separate from the normal OCS/ECS/AGA chipset path:

```text
Normal Amiga video:  CPU -> Chip RAM -> OCS/ECS/AGA -> normal display
RTG video:          CPU -> UAEGFX/Picasso96 -> RTG VRAM -> Switch framebuffer
```

RTG is optional. It is not required for ordinary ECS, OCS, or AGA games, WHDLoad, ADF, HDF, LHA, DMS, CD32 software, audio, or input.

## 2. Requirements

You need:

- A recent UAE4ALL2 HD Nintendo Switch NRO.
- A legally obtained Kickstart ROM suitable for the Amiga configuration.
- Amiga software that contains or can access Picasso96/RTG support.
- Picasso96 or compatible RTG libraries installed on the Amiga boot volume when the software requires them.
- Enough RTG VRAM for the selected resolution and pixel format.

Enabling RTG VRAM alone does not install `rtg.library`, `picasso96.library`, monitor files, or an RTG-aware application. Those files must be present in the Amiga software environment, for example on an HDF, a Workbench installation, or an installed WHDLoad package that includes RTG support.

## 3. Enable RTG VRAM on the Switch

1. Start UAE4ALL2 HD and open the **Hardware** tab.
2. Find **RTG VRAM (Picasso96)**.
3. Select one of the following values:
   - `Disabled`: RTG is unavailable and no RTG VRAM is allocated.
   - `2 MB`: recommended starting point for 640x480 and most 800x600 8/16-bit uses.
   - `4 MB`: recommended for 1024x768, 1280x720, 32-bit modes, large Workbench screens, or multiple RTG allocations.
4. Apply or save the configuration.
5. Restart the emulated Amiga if the menu requests it. A restart is recommended whenever the RTG VRAM size changes.

RTG VRAM is separate from Chip RAM and Fast RAM. The setting does not reduce the configured Chip RAM directly, although the Switch still has a finite amount of available memory.

## 4. Start an RTG program

After enabling RTG VRAM:

1. Boot the HDF, floppy, directory-mounted Workbench, or WHDLoad environment containing the RTG software.
2. Start the RTG-aware application.
3. Open the application's display, screen, or graphics preferences.
4. Select a Picasso96, UAEGFX, or RTG screen mode.
5. Select the desired resolution, depth, and refresh rate offered by the application.
6. Confirm the mode and wait for the program to open its RTG screen.

The emulator exposes common modes including 320x200, 320x240, 640x400, 640x480, 800x600, 1024x768, and 1280x720, subject to the available memory and the mode's pixel depth.

A program must actually open an RTG screen to use the RTG framebuffer. A normal ECS/OCS/AGA program will continue to use the chipset display even when RTG VRAM is enabled.

## 5. Recommended first test

For the first test, use a simple Workbench or RTG test environment:

1. Enable `2 MB` RTG VRAM.
2. Boot a Workbench/HDF environment with Picasso96 or compatible RTG libraries.
3. Select `640x480` at 16-bit, or `800x600` at 8/16-bit.
4. Open a Workbench screen or RTG-aware application.
5. Confirm that windows, text, and pointer graphics are visible.
6. Then test `1024x768` with `4 MB` if required.

If the application reports that it cannot create a screen mode, verify the RTG library installation and try a lower resolution or lower pixel depth first.

## 6. RTG and normal games

RTG can remain enabled while running software that does not use it.

Expected behavior:

- **RTG disabled + ECS/OCS/AGA game**: normal chipset video.
- **RTG enabled + ECS/OCS/AGA game**: normal chipset video; RTG remains unused.
- **RTG enabled + RTG program**: the program can open a Picasso96/UAEGFX screen.
- **RTG disabled + RTG program**: the program cannot create an RTG screen and may fall back or report an unavailable graphics device.

For maximum compatibility with old games, leave RTG disabled unless it is needed. For Workbench or RTG software, enable it before booting.

## 7. WHDLoad, ADF, and HDF behavior

RTG is not a prerequisite for the existing loading paths:

- WHDLoad ECS/OCS/AGA games work with RTG disabled.
- ADF games work with RTG disabled.
- HDF boot and filesystem access work with RTG disabled.
- LHA installation and DMS floppy images do not require RTG.
- Enabling RTG should not make a non-RTG game use RTG automatically.

Some individual WHDLoad titles may have an optional RTG mode. That depends on the title and its included files. The WHDLoad title must provide or install the appropriate Amiga RTG software itself.

## 8. Resolution and memory guidance

RTG memory requirements increase with resolution, pixel depth, virtual screen size, and the number of allocated bitmaps.

Approximate framebuffer size for one visible bitmap:

```text
width x height x bytes-per-pixel
```

Examples:

| Mode | Approximate framebuffer | Suggested RTG VRAM |
|---|---:|---:|
| 640x480, 8-bit | 300 KiB | 2 MB |
| 800x600, 8-bit | 469 KiB | 2 MB |
| 800x600, 16-bit | 938 KiB | 2 MB |
| 1024x768, 16-bit | 1.5 MiB | 4 MB |
| 1024x768, 32-bit | 3.0 MiB | 4 MB |
| 1280x720, 32-bit | 3.5 MiB | 4 MB |

The driver reserves part of the RTG memory aperture for board data, mode information, strings, and internal allocations. Therefore, the usable amount is lower than the nominal 2 MB or 4 MB value.

## 9. Pixel formats

The current Switch output path supports the common formats used by Picasso96 software:

- 8-bit CLUT/palette modes;
- 15-bit modes;
- 16-bit RGB modes;
- 24-bit RGB modes;
- 32-bit RGB/ARGB-style modes;
- Classic CRT (curved screen, scanlines, phosphor grille, vignette, and subtle RGB separation);

The final Switch display is presented through the SDL2/OpenGL-based Switch renderer, normally using an RGB565 surface before scaling and optional display shaders are applied. The Display menu also includes a Switch-compatible **Classic CRT** preset. It is a native inline GLES2 implementation with no external shader pack or runtime dependency.

## 10. Troubleshooting

### The program says “could not create screen mode”

Check the following:

1. RTG VRAM is set to `2 MB` or `4 MB`, not `Disabled`.
2. The Amiga environment contains the required Picasso96/RTG libraries and monitor configuration.
3. The selected mode is one of the modes exposed by the emulator.
4. There is enough RTG VRAM for the selected resolution and depth.
5. Restart after changing the RTG VRAM setting.
6. Try 640x480 at 16-bit before trying a larger or 32-bit mode.

### The screen opens but appears flat blue or has incorrect colors

1. Use the latest NRO build.
2. Test 640x480 or 1024x768.
3. Try changing the application's pixel depth.
4. Reopen the RTG screen after changing the mode.
5. Confirm that the selected mode is actually an RTG screen and not a chipset screen.

The Switch framebuffer path includes special handling for the FAME memory layout and 8-bit CLUT modes. Older NRO files may not contain the latest framebuffer fixes.

### The application opens but only the normal Amiga display is visible

This usually means the application is still using an OCS/ECS/AGA screen. Check the application's own display preferences and explicitly select a Picasso96, UAEGFX, or RTG screen mode.

### RTG works at low resolution but not at high resolution

Increase RTG VRAM from `2 MB` to `4 MB`, reduce the pixel depth, or reduce the virtual screen size. A 32-bit 1024x768 screen needs substantially more memory than an 8-bit 640x480 screen.

### The normal game no longer starts after enabling RTG

Set **RTG VRAM (Picasso96)** to `Disabled`, save the configuration, and restart. Then verify the Kickstart and game configuration independently. RTG is optional and should not be required by non-RTG software.

## 11. Current implementation limits

The implementation is intentionally isolated and does not replace the CPU core, JIT, ECS/OCS/AGA chipset, or normal Switch renderer. It currently provides the practical RTG paths needed by compatible software, but it should not be described as full emulation of every historical Picasso96 board feature.

The following areas may be limited, software-emulated, or unavailable depending on the application:

- board-specific hardware registers;
- hardware cursor support;
- advanced interrupt and locking behavior;
- some accelerated drawing vectors;
- uncommon pixel formats;
- applications that depend on a specific commercial graphics board rather than the UAEGFX-compatible interface.

When a vector is not accelerated, the Amiga-side library may use its software fallback. This can be slower but preserves compatibility where the library supports fallback behavior.

## 12. Disabling RTG safely

To disable RTG:

1. Exit the Amiga program or return to the emulator menu.
2. Open the **Hardware** tab.
3. Set **RTG VRAM (Picasso96)** to `Disabled`.
4. Save/apply the configuration.
5. Restart the emulated machine before launching a normal ECS/OCS/AGA title.

A reset or shutdown clears the RTG state and VRAM mapping so that the next game starts with a clean optional device state.
