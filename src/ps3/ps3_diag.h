#ifndef PS3_DIAG_H
#define PS3_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Lightweight runtime diagnostics for the PS3 port.
 *
 * The app can hang (RSX flip never completing, audio device stalling, a
 * blocking file/network operation). When that happens the log simply stops,
 * which tells us nothing about *where* it stopped. This module keeps the
 * information needed to find out:
 *
 *   - a heartbeat line every few seconds (frames, flips, audio callbacks,
 *     heap usage, current phase);
 *   - checkpoints: short static names set right before/after every operation
 *     that can block (SDL_Flip present, audio lockstep wait, menu view,
 *     dialogs, FTP);
 *   - a watchdog thread that notices when frames stop advancing and prints
 *     the checkpoint/phase the main thread is stuck in.
 */

/* Call once per presented frame (from SDL_Flip). Drives the heartbeat and
 * starts the watchdog thread on the first call. */
void ps3_diag_frame(void);

/* Duration of the blocking present/flip inside SDL_Flip, in milliseconds. */
void ps3_diag_present_time(unsigned int ms);

/* Name of the operation currently running; must be a static string. */
void ps3_diag_checkpoint(const char *name);

/* Coarse phase shown in heartbeat/stall lines (menu tab, game, splash);
 * must be a static string. Logged when it changes. */
void ps3_diag_phase(const char *phase);

/* Audio callback came in (called from the SDL audio thread). */
void ps3_diag_audio_cb(void);

/* The emulator waited 'ms' milliseconds for an audio callback without one
 * arriving. */
void ps3_diag_audio_stall(unsigned int ms);

/* Hooks called by the patched SDL2 PSL1GHT renderer: a checkpoint for every
 * step of SDL_RenderPresent (log-free, several calls per frame) and one call
 * per dropped flip whose wait timed out.  Declared here so the names stay in
 * sync with src/render/psl1ght/SDL_PSL1GHTrender.c. */
void ps3_sdl_render_cp(const char *what);
void ps3_sdl_flip_timeout(unsigned int ms);

/* Hook called by the patched renderer each time the firmware flip FIFO is full
 * and the command had to be rewound instead of blocking inside the firmware. */
void ps3_sdl_fifo_blocked(unsigned int ms);

/* Called from inside the emulator loop (m68k_run), a few dozen times per
 * second.  The watchdog thread notices a *blocked* main thread; this notices a
 * main thread that keeps running but never reaches a vertical blank (a game
 * wedged in an emulated loop, for example).  Both give the console back to the
 * XMB instead of leaving it stuck. */
void ps3_diag_tick(void);

/* Name of the game currently running, implemented by the PS3 GUI layer and
 * shown in the stall report so a freeze on real hardware can be traced back to
 * the title that was launched. Returns a static string, never NULL. */
const char *ps3_diag_game_name(void);

#ifdef __cplusplus
}
#endif

#endif
