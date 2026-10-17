# UAE4ALL2 HD — PlayStation 3 Edition (v1.00)

A complete, high-performance Amiga emulator for PlayStation 3 homebrew, supporting Amiga 500, 500+, 600, 1200 (AGA), and CD32 hardware.

Built with [PSL1GHT](https://github.com/ps3dev/PSL1GHT) as a native homebrew package, with a custom RSX-accelerated renderer and full 1080p menus.

This repository provides the complete PlayStation 3 source code and ready-to-run release packages:
- **`uae4all2hd_PS3_v1.00.gnpdrm.pkg`**: Signed retail NPDRM package, installable from the XMB.

Release notes and version history are available in [`CHANGELOG.md`](CHANGELOG.md). License information is provided in [`copying`](copying).

---

## Quick Start: Play in 3 Simple Steps

### 1. Install the Package
- Download the latest **`uae4all2hd_PS3_v1.00.pkg`** from the [Releases](https://github.com/theheroGAC/UAE4ALL2-HD-PS3/releases) page.
- Put the file on a USB stick (or download it to `/dev_hdd0` via any setup you already use).
- On your PS3 XMB, choose **Install Package Files** and select the package.
- Launch **UAE4All2 HD** from the XMB.

> [!NOTE]
> A PS3 with the ability to install homebrew packages is required.

### 2. Add Kickstart BIOS ROMs
Like the original Amiga computers, the emulator requires Kickstart ROM files to boot. Place your legally acquired Kickstart ROMs into:
```text
/dev_hdd0/game/UAEH00001/USRDIR/kickstarts/
```
The app automatically imports them into `/dev_hdd0/uae4all/kickstarts/` (persistent storage that survives reinstalls). You can also transfer the ROMs with the built-in **FTP server** (see below) — no USB stick needed.

> [!TIP]
> The two essential Kickstart files that boot the vast majority of games are:
> - `kick13.rom` (Kickstart 1.3 for Amiga 500 games)
> - `kick31.rom` (Kickstart 3.1 for Amiga 1200 AGA games and WHDLoad titles)

### 3. Add Games and Launch
- Put your games inside `/dev_hdd0/uae4all/whdload/` (WHDLoad `.lha` packages, auto-extracted on launch) or copy floppy images (`.adf`) to a folder of your choice.
- Open the **Game Library** and press **X** to launch with one click.

---

## Supported Systems and File Formats

This port uses the same emulation core as the other UAE4ALL2 HD editions, so the full spectrum of Amiga hardware and disk formats is supported:

### Supported Amiga Systems
- **Amiga 500 & 500+**: OCS and ECS chipsets, Motorola 68000 CPU, 512 KB Chip RAM + Fast RAM expansion.
- **Amiga 600**: ECS chipset, Motorola 68000 CPU, IDE hard disk support, 2 MB Chip RAM + Fast RAM.
- **Amiga 1200 (AGA)**: AGA 256-color chipset, Motorola 68020 CPU, 2 MB Chip RAM + up to 8 MB Fast RAM.
- **Amiga CD32**: CD-ROM console emulation with Akiko support (Chunky-to-Planar conversion) and CD audio playback.

### Supported File Formats
- **Floppy images**: **ADF / ADZ** (and `.gz` compressed ADF) across 4 virtual drives (DF0–DF3).
- **WHDLoad games**: native high-speed **LHA** decompressor with automatic Slave launching, automatic hardware profile (A500/A1200) and 1-click execution from the Library or the WHDLoad tab.
- **Hard disks**: HDF hardfiles, compressed CHD hardfiles, and directory mounts (`DH0:`) so you can point the Amiga at any PS3 folder.
- **CD images**: ISO / BIN+CUE and compressed **CHD** with multi-track support.
- **Multi-disk**: M3U playlist support for multi-floppy games with in-game disk swapping.
- **Save states**: full-state snapshots with screenshot previews across 5 slots per game.

---

## Main Features

- **1-Click Game Library With Artwork**: browse WHDLoad, ADF, HDF, and CD32 games in one unified screen, with cover art, year, publisher, genre, and player count metadata.
- **Full 1080p Native Menus**: all menu screens are rendered directly onto the PS3 framebuffer at 1920×1080 (in-game output is scaled and filtered with hardware shaders).
- **Stable 60 FPS Menus**: pure caching of game lists, covers, and metadata in memory — browsing hundreds of games is instant and has no disk-stall or micro-stutter.
- **Automatic Hardware Profiles**: WHDLoad packages are detected as Amiga 500 (OCS/ECS) or Amiga 1200 (AGA) and the proper CPU / memory layout is applied before boot.
- **Built-in FTP Server**: transfer games, kickstarts, covers, and saves over Wi-Fi or LAN without pulling out the internal HDD.
- **Display Filters & Aspect Ratios**: CRT-style shaders, Sharp Bilinear, Scale2x; authentic 4:3, 5:4, and widescreen 16:9 scaling.
- **Authentic Drive Sounds**: real floppy motor / seek and hard drive access samples for maximum nostalgia factor.
- **Configurable Audio**: adjustable stereo separation and resampling for headphones, always at PS3-grade 48 kHz storage.

---

## PS3 Folder Structure

All user data lives on the console HDD in two places:

```text
/dev_hdd0/game/UAEH00001/USRDIR/     # App installation directory
├── EBOOT.BIN                        # Main application binary
├── kickstarts/                      # Put your Kickstart ROMs here (BIOS files)
├── ps3log.txt                       # Rolling diagnostics log (see below)
├── data/                            # System data, fonts, sounds, covers db (do not delete)
├── screenshots/                     # User captured gameplay screenshots
├── saves/ & thumbs/                 # Save states and previews
└── conf/                            # Per-game saved configurations

/dev_hdd0/uae4all/                   # Persistent data (survives app reinstalls)
├── kickstarts/                      # Importing zone — auto-populated from USRDIR
├── whdload/                         # Installed WHDLoad games
└── ...
```

The emulator works with an internal HDD only; nothing needs to be on network shares unless you use FTP.

---

## Built-in FTP Server

The emulator includes a full-featured FTP server (RFC-compatible, works with FileZilla, WinSCP, Cyberduck, and most phone apps):

1. Be sure your PS3 is connected to your network (Wi-Fi from System Settings).
2. Open the **System** tab and select **FTP File Transfer**.
3. The screen shows a live address such as `ftp://192.168.1.15:21`.
4. On your PC, enter the IP address from the screen and **port 21** (standard FTP port), connect as anonymous or any user/password.
5. Press **O** to stop the server and return to the menu.

Access is read/write over the whole PS3 file tree, so be careful what you move around.

---

## Diagnostics Log

If you hit an issue, the app keeps a persistent rolling log at:
```text
/dev_hdd0/game/UAEH00001/USRDIR/ps3log.txt
```
The log flushes every line and rotates at 4 MB to `ps3log.txt.old`, so the tail of a crashed session survives a restart. Attach it when reporting a bug.

---

## Frequently Asked Questions (FAQ)

### 1. A WHDLoad game shows a black screen or fails to boot. What should I check?
- Make sure `kick31.rom` (512 KB) is in the `kickstarts/` folder — WHDLoad games need it.
- Check the WHDLoad tab for the correct **Hardware** profile (A500 vs A1200).

### 2. How do I connect to the PS3 from my PC via FTP?
1. Connect the PS3 to your network (Wi-Fi or LAN cable).
2. In UAE4All2 HD, open the **System** tab and select **FTP File Transfer**.
3. Note the displayed IP address and port (e.g. `ftp://192.168.1.15:21`).
4. In FileZilla/WinSCP, enter the IP in the Host field, `21` in the Port field, and connect (anonymous).
5. Copy games, kickstarts, covers, and saves directly over the network.

### 3. How do multi-disk games work (.m3u)?
Create a text file with an `.m3u` extension, listing the disks in order, one per line. Load the `.m3u` into DF0 and swap disks from the in-game menu or with the mapped hotkeys.

### 4. Where are my saved games / savestates?
Save states are in `/dev_hdd0/game/UAEH00001/USRDIR/saves/` (and previews in `thumbs/`). Per-game configuration snapshots live in `conf/`.

### 5. The screen freezes when I quit from the menu after starting a game.
Use the **System → Exit** entry in the in-game menu — it triggers the standard PS3 termination and returns you to the XMB. If you ever find a case that doesn't come back to the XMB, attach `ps3log.txt` from the app directory.

---

## Building from Source

Prerequisites (on Windows via WSL, or any Linux box):
- [PSL1GHT](https://github.com/ps3dev/PSL1GHT) toolchain installed under `/mnt/d/ps3dev-sdk` (or export `PS3DEV` yourself).
- `python3` with `Pillow` for icon generation (optional, falls back to `ps3data/ICON0.PNG`).

One-command build (configures CMake, compiles, makes SELF, stages and signs the PKG):
```bash
bash tools/build_pkg_fixed.sh
```
Expected outputs:
- `uae4all2hd_PS3_v1.00.pkg` — unsigned intermediate
- `uae4all2hd_PS3_v1.00.gnpdrm.pkg` — final retail NPDRM package

Quick syntax-only compile check of endianness-sensitive sources with the real PS3 toolchain (no linking):
```bash
bash tools/check_ps3_compile.sh [extra files...]
```

The standard CMake build passes `-DBUILD_PS3=ON` to the configure step and produces `build-ps3/uae4all2.elf`, which the release script strips, converts to a NPDRM SELF (`make_self_npdrm`), and packages with the PSL1GHT `pkg` tool.

---

## Credits and Acknowledgements

- **Original UAE4ALL / UAE4ALL2 authors**: Chui, john4p, TomB, notaz, Bernd Schneider, Toni Wilen, Pickle, smoku, AnotherGuest, Anonymous engineer, finkel, Lubomyr, pelya.
- **Cpasjuste**: original Vita port, SDL-Vita shaders and performance optimizations.
- **rsn8887**: Vita and Switch ports, controller mappings and UAE4ALL2 enhancements.
- **theheroGAC**: UAE4ALL2 HD project lead — HD menu design, WHDLoad integration, CD32 & HDF enhancements, and this PlayStation 3 port ([UAE4ALL2-HD-VITA](https://github.com/theheroGAC/UAE4ALL2-HD-VITA), [UAE4ALL2-HD-SWITCH](https://github.com/theheroGAC/UAE4ALL2-HD-SWITCH)).
- **Bert Jahn (Wepl)**: author of WHDLoad (http://www.whdload.de).
- **Timothy Lottes**: CRT-Lottes shader. **aliaspider**: CRT-Easymode shader. **Andrea Mazzoleni**: Scale2x algorithm.
- **Software Preservation Society**: CAPS / IPF floppy decoding.
- **Romain Tisserand & MAME Team**: libchdr (CHD compression).
- **PSL1GHT community**: SDK, toolchain, and years of reverse engineering documentation.

---

## Legal Notice and License

UAE4ALL2 HD is intended for use with legally acquired Amiga software and Kickstart ROMs. Kickstart ROMs are **not included** in the package and must be obtained separately. Amiga is a registered trademark of its respective owners. This software is released under the GNU General Public License v2 (GPL-2.0). See [`copying`](copying) for details.
