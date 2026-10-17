#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>

#include <SDL.h>
#include <rsx/gcm_sys.h>
#include <lv2/process.h>

#include "ps3_diag.h"
#include "ps3_log.h"

#define PS3_DIAG_PERIOD_MS 3000u
#define PS3_DIAG_PERIOD_S  (PS3_DIAG_PERIOD_MS / 1000u)

#define PS3_DIAG_MAX_STALL_S 20u
#define PS3_DIAG_MAX_STALL_MS (PS3_DIAG_MAX_STALL_S * 1000u)

#define PS3_DIAG_TICK_EVERY 64u

static volatile unsigned int s_frames;
static volatile const char *s_checkpoint = "boot";
static const char *s_phase = "boot";

static unsigned int s_audio_cb;
static volatile unsigned int s_audio_stalls;
static unsigned int s_flip_count;
static unsigned int s_flip_max_ms;
static unsigned int s_present_max_ms;
static unsigned int s_flip_timeouts;
static unsigned int s_fifo_blocked;
static volatile unsigned int s_stalls;

static volatile unsigned int s_arena_kb;
static volatile unsigned int s_used_kb;

extern unsigned m68k_get_pc(void);

extern const char *ps3_diag_game_name(void);

static gcmControlRegister *s_ctrl;

static volatile unsigned int s_last_frame_ms;
static unsigned int s_ticks;

static unsigned int s_start_ms;
static unsigned int s_last_report_ms;
static int s_started;
static int s_watchdog_state;

static unsigned int heap_arena_kb(void)
{
	struct mallinfo mi = mallinfo();
	return (unsigned int)(mi.arena / 1024);
}

static unsigned int heap_used_kb(void)
{
	struct mallinfo mi = mallinfo();
	return (unsigned int)(mi.uordblks / 1024);
}

static void *ps3_diag_watchdog(void *arg)
{
	unsigned int last_frames = 0;
	unsigned int stuck_s = 0;

	(void)arg;

	for (;;) {
		unsigned int frames;

		sleep(PS3_DIAG_PERIOD_S);

		frames = s_frames;
		if (frames == last_frames) {
			stuck_s += PS3_DIAG_PERIOD_S;
			s_stalls++;			if (s_ctrl) {		ps3_write_log("[PS3] STALL %us: frames=%u game=%s m68k_pc=%08x checkpoint=%s phase=%s flips=%u flip_timeouts=%u fifo_blocked=%u present_max=%ums audio_cb=%u audio_stalls=%u arena=%uKB used=%uKB rsx_put=%u rsx_get=%u rsx_ref=%u\n",
			              stuck_s, frames,
			              ps3_diag_game_name() ? ps3_diag_game_name() : "-",
			              (unsigned int)m68k_get_pc(),
			              s_checkpoint ? s_checkpoint : "?",
			              s_phase ? s_phase : "?",
			              s_flip_count, s_flip_timeouts, s_fifo_blocked,
			              s_present_max_ms,
			              s_audio_cb, s_audio_stalls,
			              s_arena_kb, s_used_kb,
			              (unsigned int)s_ctrl->put,
			              (unsigned int)s_ctrl->get,
			              (unsigned int)s_ctrl->ref);
		} else {
			ps3_write_log("[PS3] STALL %us: frames=%u game=%s m68k_pc=%08x checkpoint=%s phase=%s flips=%u flip_timeouts=%u fifo_blocked=%u present_max=%ums audio_cb=%u audio_stalls=%u arena=%uKB used=%uKB\n",
			              stuck_s, frames,
			              ps3_diag_game_name() ? ps3_diag_game_name() : "-",
			              (unsigned int)m68k_get_pc(),
			              s_checkpoint ? s_checkpoint : "?",
			              s_phase ? s_phase : "?",
			              s_flip_count, s_flip_timeouts, s_fifo_blocked,
			              s_present_max_ms,
			              s_audio_cb, s_audio_stalls,
			              s_arena_kb, s_used_kb);
		}

			if (stuck_s >= PS3_DIAG_MAX_STALL_S && (stuck_s % PS3_DIAG_MAX_STALL_S) < PS3_DIAG_PERIOD_S) {
				ps3_write_log("[PS3] watchdog: no frame for %us (checkpoint=%s phase=%s)\n",
				              stuck_s,
				              s_checkpoint ? s_checkpoint : "?",
				              s_phase ? s_phase : "?");
			}
		} else {
			stuck_s = 0;
			last_frames = frames;
		}
	}

	return NULL;
}

void ps3_diag_tick(void)
{
	unsigned int now;

	if (!s_started)
		return;
	if (++s_ticks < PS3_DIAG_TICK_EVERY)
		return;
	s_ticks = 0;

	now = SDL_GetTicks();
	if (now - s_last_frame_ms < PS3_DIAG_MAX_STALL_MS)
		return;

	ps3_write_log("[PS3] tick: no frame for %ums (checkpoint=%s phase=%s m68k_pc=%08x game=%s)\n",
	              now - s_last_frame_ms,
	              s_checkpoint ? s_checkpoint : "?",
	              s_phase ? s_phase : "?",
	              (unsigned int)m68k_get_pc(),
	              ps3_diag_game_name() ? ps3_diag_game_name() : "-");
	s_last_frame_ms = now;
}

void ps3_diag_frame(void)
{
	unsigned int now = SDL_GetTicks();

	s_last_frame_ms = now;

	if (!s_started) {
		s_started = 1;
		s_start_ms = now;
		s_last_report_ms = now;

	if (!s_ctrl) {
		s_ctrl = gcmGetControlRegister();
		ps3_write_log("[PS3] diag: RSX control register %s (build v1.00 %s %s)\n",
		              s_ctrl ? "ready" : "unavailable", __DATE__, __TIME__);
	}

		if (s_watchdog_state == 0) {
			pthread_t tid;

			s_watchdog_state = 1;
			if (pthread_create(&tid, NULL, ps3_diag_watchdog, NULL) == 0) {
				pthread_detach(tid);
				s_watchdog_state = 2;
				ps3_write_log("[PS3] diag: watchdog thread started\n");
			} else {
				s_watchdog_state = -1;
				ps3_write_log("[PS3] diag: watchdog thread unavailable\n");
			}
		}
	}

	s_frames++;

	if (now - s_last_report_ms >= PS3_DIAG_PERIOD_MS) {
		s_last_report_ms = now;
		s_arena_kb = heap_arena_kb();
		s_used_kb = heap_used_kb();
		ps3_write_log("[PS3] HB t=%us frames=%u flips=%u flip_max=%ums flip_timeouts=%u fifo_blocked=%u audio_cb=%u stalls=%u audio_stalls=%u arena=%uKB used=%uKB phase=%s\n",
		              (now - s_start_ms) / 1000u, s_frames, s_flip_count,
		              s_flip_max_ms, s_flip_timeouts, s_fifo_blocked, s_audio_cb, s_stalls, s_audio_stalls,
		              s_arena_kb, s_used_kb,
		              s_phase ? s_phase : "?");
		s_flip_max_ms = 0;
		s_present_max_ms = 0;
	}
}

void ps3_diag_present_time(unsigned int ms)
{
	s_flip_count++;
	if (ms > s_flip_max_ms)
		s_flip_max_ms = ms;
	if (ms > s_present_max_ms)
		s_present_max_ms = ms;

	if (ms > 500u) {
		ps3_write_log("[PS3] slow present: SDL_RenderPresent took %ums (phase=%s checkpoint=%s)\n",
		              ms, s_phase ? s_phase : "?",
		              s_checkpoint ? s_checkpoint : "?");
	}
}

void ps3_diag_checkpoint(const char *name)
{
	s_checkpoint = name ? name : "?";
}

void ps3_sdl_render_cp(const char *what)
{
	ps3_diag_checkpoint(what);
}

void ps3_sdl_flip_timeout(unsigned int ms)
{
	s_flip_timeouts++;

	if (s_flip_timeouts <= 5 || (s_flip_timeouts % 50) == 0) {
		ps3_write_log("[PS3] flip timeout #%u: flip not consumed in %ums (phase=%s checkpoint=%s)\n",
		              s_flip_timeouts, ms, s_phase ? s_phase : "?",
		              s_checkpoint ? s_checkpoint : "?");
	}
}

void ps3_sdl_fifo_blocked(unsigned int ms)
{
	s_fifo_blocked++;

	if (s_fifo_blocked <= 5 || (s_fifo_blocked % 50) == 0) {
		ps3_write_log("[PS3] fifo blocked #%u: flip FIFO full for %ums (phase=%s checkpoint=%s)\n",
		              s_fifo_blocked, ms, s_phase ? s_phase : "?",
		              s_checkpoint ? s_checkpoint : "?");
	}
}

void ps3_diag_phase(const char *phase)
{
	if (!phase)
		return;
	if (s_phase == phase)
		return;
	s_phase = phase;
	ps3_write_log("[PS3] phase: %s\n", phase);
}

void ps3_diag_audio_cb(void)
{
	s_audio_cb++;
}

void ps3_diag_audio_stall(unsigned int ms)
{
	s_audio_stalls++;

	if (s_audio_stalls <= 5 || (s_audio_stalls % 50) == 0) {
		ps3_write_log("[PS3] audio stall #%u: no callback for %ums (phase=%s checkpoint=%s)\n",
		              s_audio_stalls, ms, s_phase ? s_phase : "?",
		              s_checkpoint ? s_checkpoint : "?");
	}
}
