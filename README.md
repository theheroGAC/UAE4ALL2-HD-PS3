# UAE4ALL2 HD — PlayStation 3 Edition (v1.01)

A complete, high-performance Amiga emulator for PlayStation 3 homebrew, supporting Amiga 500, 500+, 600, 1200 (AGA), CDTV and CD32 hardware.

Built with [PSL1GHT](https://github.com/ps3dev/PSL1GHT) as a native homebrew package, with a custom RSX-accelerated renderer and full 1080p menus.

This repository provides the complete PlayStation 3 source code and ready-to-run release packages:
- **`uae4all2hd_PS3_v1.01.gnpdrm.pkg`**: Signed retail NPDRM package, installable from the XMB.

Release notes and version history are available in [`CHANGELOG.md`](CHANGELOG.md). License information is provided in [`copying`](copying).

---

## Quick Start: Play in 3 Simple Steps

### 1. Install the Package
- Download the latest **`uae4all2hd_PS3_v1.01.pkg`** from the [Releases](https://github.com/theheroGAC/UAE4ALL2-HD-PS3/releases) page.
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
The app automatically imports them into `/dev_hdd0/uae4all/kickstarts/` (persistent storage that survives reinstalls). You can also transfer the ROMs with the built-in **FTP server** or from an **external USB drive** (see below) — no manual HDD surgery needed.

> [!TIP]
> The two essential Kickstart files that boot the vast majority of games are:
> - `kick13.rom` (Kickstart 1.3 for Amiga 500 games)
> - `kick31.rom` (Kickstart 3.1 for Amiga 1200 AGA games and WHDLoad titles)

The app also accepts the Amiga Forever / TOSEC names (`amiga-os-130.rom`, `kick34005.A500`, `kick40068.A1200`, …) and verifies sizes with the reference MD5 checksums listed in the [Kickstart ROMs table](#kickstart-roms-reference) below.

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
- **Amiga CDTV**: CD-ROM media player profile with the CDTV extended ROM.
- **Amiga CD32**: CD-ROM console emulation with Akiko support (Chunky-to-Planar conversion) and CD audio playback.

### Supported File Formats
- **Floppy images**: **ADF / ADZ** (and `.gz` compressed ADF) across 4 virtual drives (DF0–DF3).
- **IPF images**: authentic **CAPS / SPS** preservation images, including many copy-protected original releases, decoded with the bundled CAPSImg library.
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
- **Turbo Fast-Forward With On-Screen Indicator**: hold the fast-forward control for instant speed-up — the screen shows `>> TURBO >>` while active and `TURBO OFF` on release, in the same style as the Vita edition.
- **External USB Drive Support**: browse game folders on `/dev_usb000`–`/dev_usb003` straight from the emulator menus.
- **Built-in FTP Server**: transfer games, kickstarts, covers, and saves over Wi-Fi or LAN without pulling out the internal HDD.
- **Built-in On-Screen Keyboard**: enter configuration names, savestate names and WHDLoad arguments with an in-app QWERTY keyboard (D-pad navigation, SHIFT toggle, CIRCLE cancels, TRIANGLE confirms) — no system overlay dependency.
- **Display Filters & Aspect Ratios**: CRT-style shaders, Sharp Bilinear, Scale2x; authentic 4:3, 5:4, and widescreen 16:9 scaling.
- **Authentic Drive Sounds**: real floppy motor / seek and hard drive access samples for maximum nostalgia factor.
- **Configurable Audio**: adjustable stereo separation and resampling for headphones, always at PS3-grade 48 kHz storage.

---

## External USB Drives

Games and ROMs can live entirely on a USB stick — no space is needed on the internal HDD:

- Open the **Floppy** browser in `./roms` (or the Hard Disk browser in the persistent folder) and a **USB 1 / USB 2 / …** entry appears for every detected `/dev_usb000`–`/dev_usb003` drive. Enter it to browse the stick contents.
- The WHDLoad manager and the Kickstart loader also scan the root of each USB drive (and `/dev_usbXXX/uae4all/…` subfolders), so a single stick can carry WHDLoad packages, `.adf` sets, and BIOS files together.
- USB folders are read like any PS3 folder; heavy titles stream faster from `/dev_hdd0`, but most Amiga games load fine from USB.

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

The emulator can work fully from the internal HDD; external USB drives are also supported as described above, and network transfers go through the FTP server.

---

## Built-in FTP Server

The emulator includes a full-featured FTP server (RFC-compatible, works with FileZilla, WinSCP, Cyberduck, and most phone apps):

1. Be sure your PS3 is connected to your network (Wi-Fi from System Settings).
2. Open the **System** tab and select **FTP File Transfer**.
3. The server always tries the standard **port 21** first; the PS3 system only allows system-level services to bind ports below 1024, so the server then moves to the first available fallback port (5000, 2121, 8021).
4. The screen shows a live address with the **actual port in use**, such as `ftp://192.168.1.15:5000` — connect to exactly the address shown.
5. Enter the IP and displayed port in your FTP client, connect as anonymous or any user/password.
6. Press **O** to stop the server and return to the menu.

Access is read/write over the whole PS3 file tree (and any USB drive), so be careful what you move around. The diagnostics log records each bind attempt with its system error code. The console IP is read from the netctl service (`netCtlGetState` + `netCtlGetInfo(IP_ADDRESS)`): the probe runs in the FTP listener thread, never on the UI/menu thread, and stays covered by the `net_cp=` watchdog checkpoints. `getsockname`/`getpeername` now call the real LV2 `sys_net` syscalls (703/704); the previous local stubs always answered `ENOSYS`, which is why the IP never surfaced and stayed on "WAITING FOR WI-FI...".

---

## Kickstart ROMs Reference

The Hardware tab accepts both the classic UAE4ALL names and the Amiga Forever / TOSEC names. The emulator probes the aliases in order (case-insensitively) and uses the first file found in the app kickstarts folder, the persistent folder, or an external USB drive.

Kickstart ROMs are copyrighted. Do not distribute them with this project. Verify your files with the official MD5 checksums below.

| System | Version | Filename (UAE4ALL) | Amiga Forever / TOSEC alias | Size | MD5 |
|---|---|---|---|---|---|
| A1000 | KS v1.1 rev 31.034 NTSC | `kick31034.A1000` | `amiga-os-110-ntsc.rom` | 262144 | `0b8442c311caa54fb12ec88eaaa9facf` |
| A1000 | KS v1.1 rev 32.034 PAL | `kick32034.A1000` | `amiga-os-110-pal.rom` | 262144 | `1fa1f93d3d7b51271dd1356b8b2b45a9` |
| A500-A2000 | KS v1.2 rev 33.180 | `kick12.rom`, `kick33180.A500` | `amiga-os-120.rom` | 262144 | `85ad74194e87c08904327de1a9443b7a` |
| A500-A2000 | KS v1.3 rev 34.005 | `kick13.rom`, `kick34005.A500` | `amiga-os-130.rom` | 262144 | `82a21c1890cae844b3df741f2762d48d` |
| A500+ | KS v2.04 rev 37.175 | `kick20.rom`, `kick37175.A500` | `amiga-os-204.rom` | 524288 | `dc10d7bdd1b6f450773dfb558477c230` |
| A600 | KS v2.05 rev 37.350 | `kick37350.A600` | `amiga-os-205-a600.rom` | 524288 | `465646c9b6729f77eea5314d1f057951` |
| A600-A2000 | KS v3.1 rev 40.063 | `kick40063.A600` | `amiga-os-310-a600.rom` | 524288 | `e40a5dfb3d017ba8779faba30cbd1c8e` |
| A1200 | KS v3.0 rev 39.106 | `kick39106.A1200` | `amiga-os-300-a1200.rom` | 524288 | `b7cc148386aa631136f510cd29e42fc3` |
| A1200 | KS v3.1 rev 40.068 | `kick31.rom`, `kick40068.A1200` | `amiga-os-310-a1200.rom` | 524288 | `646773759326fbac3b2311fd8c8793ee` |
| A4000 | KS v3.0 rev 39.106 | `kick39106.A4000` | `amiga-os-300-a4000.rom` | 524288 | `9b8bdd5a3fd32c2a5a6f5b1aefc799a5` |
| A4000 | KS v3.1 rev 40.068 | `kick40068.A4000` | `amiga-os-310-a4000.rom` | 524288 | `9bdedde6a4f33555b4a270c8ca53297d` |
| CD32 | KS + Extended v3.1 rev 40.060 | `kick40060.CD32` | combined 1 MiB image | 1048576 | `f2f241bf094168cfb9e7805dc2856433` |
| CD32 | KS v3.1 rev 40.060 | `kick40060.CD32` | `amiga-os-310-cd32.rom` | 524288 | `5f8924d013dd57a89cf349f4cdedc6b1` |
| CD32 | Extended ROM rev 40.060 | `kick40060.CD32.ext` | `amiga-os-310-cd32-ext.rom` | 524288 | `bb72565701b1b6faece07d68ea5da639` |
| CDTV | CDTV Extended ROM v1.0 | `kick34005.CDTV` | `amiga-os-130-cdtv-ext.rom` | 262144 | `89da1838a24460e4b93f4f0c5d92d48d` |

For the CD32 preset, both ROMs are required:

```text
kick40060.CD32
kick40060.CD32.ext
```

`kick40060.CD32` is the 512 KiB main CD32 Kickstart ROM and `kick40060.CD32.ext` is the 512 KiB CD32 Extended ROM. The extended ROM is what selects the CD32 hardware path and maps Akiko at `$B80000`; an A1200 `kick31.rom` cannot replace either CD32 file. A single combined 1 MiB `kick40060.CD32` image is also recognized (filenames are matched case-insensitively on the PS3).

The emulator also accepts `kickcustom.rom` / `custom.rom` for a user ROM, diagnostic ROM images (`diagrom.rom`, `kick32.rom`, `kick322.rom`, …), and `aros-amiga-m68k-rom.bin` / `aros.rom` for the AROS fallback ROM (`aros-amiga-m68k-ext.bin` / `aros-ext.bin` is the matching extended ROM). CDTV extended images are picked up from `amiga-os-130-cdtv-ext.rom`, `amiga-os-130-cdtv.rom`, or `cdtv.rom`.

---

## Diagnostics Log

If you hit an issue, the app keeps a persistent rolling log at:
```text
/dev_hdd0/game/UAEH00001/USRDIR/ps3log.txt
```
The log flushes every line and rotates at 4 MB to `ps3log.txt.old`, so the tail of a crashed session survives a restart. Each session begins with the build banner (e.g. `build v1.01`) so reported logs identify the exact running version. Attach it when reporting a bug.

---

## Frequently Asked Questions (FAQ)

### 1. A WHDLoad game shows a black screen or fails to boot. What should I check?
- Make sure `kick31.rom` (512 KB) is in the `kickstarts/` folder — WHDLoad games need it.
- Check the WHDLoad tab for the correct **Hardware** profile (A500 vs A1200).

### 2. How do I connect to the PS3 from my PC via FTP?
1. Connect the PS3 to your network (Wi-Fi or LAN cable).
2. In UAE4All2 HD, open the **System** tab and select **FTP File Transfer**.
3. Note the displayed address **including the port** (e.g. `ftp://192.168.1.15:5000`).
4. In FileZilla/WinSCP, enter the IP in the Host field, the **displayed port** in the Port field, and connect (anonymous).
5. Copy games, kickstarts, covers, and saves directly over the network.

### 3. Can I play my IPF dumps?
Yes — IPF (SPS/CAPS) images are supported natively. Load a `.ipf` file into any floppy slot exactly like an `.adf`. Protected originals and multi-dump files (`Disk1a.ipf`, `Disk1b.ipf`, …) work; combine them in an `.m3u` for auto-swapping.

### 4. My games are on a USB stick. What is the easiest setup?
Create a `uae4all/whdload` and/or `uae4all/kickstarts` folder on the drive (or just dump files at its root), plug it in, and browse it from the **USB** entries in the disk browsers — WHDLoad and BIOS files are found automatically on `/dev_usb000`–`/dev_usb003`.

### 5. Where are my saved games / savestates?
Save states are in `/dev_hdd0/game/UAEH00001/USRDIR/saves/` (and previews in `thumbs/`). Per-game configuration snapshots live in `conf/`.

### 6. The screen freezes when I quit from the menu after starting a game.
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
- `uae4all2hd_PS3_v1.01.pkg` — unsigned intermediate
- `uae4all2hd_PS3_v1.01.gnpdrm.pkg` — final retail NPDRM package

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
