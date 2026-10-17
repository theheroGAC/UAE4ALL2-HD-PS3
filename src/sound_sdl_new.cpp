 /*
  * Minimalistic sound.c implementation for gp2x
  * (c) notaz, 2007
  */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef __PS3__
#include <semaphore.h>
#endif
#include <pthread.h>
#include <errno.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "memory-uae.h"
#include "debug_uae4all.h"
#include "audio.h"
#include "gensound.h"
#include "sound.h"
#include "custom.h"
#include "menu_config.h"
#include "cdrom.h"
#include "disk_sound.h"
#include "midi_synth.h"

#include "thread.h"
#include <SDL.h>

#ifdef __PS3__
#include "ps3_diag.h"
#endif

#ifdef ANDROIDSDL
#include <android/log.h>
#endif

extern unsigned long next_sample_evtime;

int produce_sound=0;
int changed_produce_sound=0;

unsigned int sound_rate=DEFAULT_SOUND_FREQ;

#if !defined(__PS3__)
#define SOUND_USE_SEMAPHORES
#endif
#define SOUND_BUFFERS_COUNT 16
static uae_u16 sndbuffer[SOUND_BUFFERS_COUNT][SNDBUFFER_LEN*DEFAULT_SOUND_CHANNELS+32] UAE4ALL_ALIGN;
unsigned n_callback_sndbuff, n_render_sndbuff;
static volatile int sound_write_idx = 2;
static volatile int sound_read_idx = 0;
uae_u16 *sndbufpt = sndbuffer[2];
uae_u16 *render_sndbuff = sndbuffer[2];

extern void gp2x_stop_sound();

#ifdef NO_SOUND


void finish_sound_buffer (void) {  }

int setup_sound (void) { sound_available = 0; return 0; }

void close_sound (void) { }

int init_sound (void) { return 0; }

void pause_sound (void) { }

void resume_sound (void) { }

void uae4all_init_sound(void) { }

void uae4all_play_click(void) { }

void uae4all_pause_music(void) { }

void uae4all_resume_music(void) { }

#else


static int have_sound = 0;

void sound_default_evtime(void)
{
#ifdef __PS3__
	sound_rate = 48000;
#endif
	int pal = (beamcon0 & 0x20) || !mainMenu_ntsc;
	if (pal)
		scaled_sample_evtime=(MAXHPOS_PAL*313*VBLANK_HZ_PAL*CYCLE_UNIT)/sound_rate;
	else
		scaled_sample_evtime=(MAXHPOS_NTSC*MAXVPOS_NTSC*VBLANK_HZ_NTSC*CYCLE_UNIT)/sound_rate + 1;

	schedule_audio();
}


static int s_oldrate = 0, s_oldbits = 0, s_oldstereo = 0;
static int sound_thread_active = 0, sound_thread_exit = 0;
#ifdef SOUND_USE_SEMAPHORES
static uae_sem_t sound_sem, callback_sem;
#endif

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
static int cnt = 0;

#ifdef __PS3__
#define PS3_AUD_FIFO_FRAMES 16384u
#define PS3_AUD_FIFO_MASK   (PS3_AUD_FIFO_FRAMES - 1u)
static uae_s16 ps3_aud_fifo[PS3_AUD_FIFO_FRAMES][2];
static volatile unsigned int ps3_aud_wr = 0;
static volatile unsigned int ps3_aud_rd = 0;
static unsigned int ps3_aud_frac_q16 = 0;
static unsigned int ps3_aud_rate_q16 = 1u << 16;
static unsigned int ps3_aud_last_push_ms = 0;
static uae_s16 ps3_aud_hold_l = 0, ps3_aud_hold_r = 0;
static unsigned int ps3_aud_underruns = 0;
#endif

static void sound_thread_mixer(void *ud, Uint8 *stream, int len)
{
	if (sound_thread_exit) return;
	sound_thread_active = 1;

#ifdef __PS3__
	extern int ps3_fast_forward;
	if (ps3_fast_forward) {
		ps3_aud_rd = ps3_aud_wr;
		memset(stream, 0, len);
		return;
	}
	ps3_diag_audio_cb();
	ps3_diag_checkpoint("snd:callback");
#endif

#ifdef SOUND_USE_SEMAPHORES
	uae_sem_wait(&sound_sem);
	uae_sem_post(&callback_sem);
#endif
	cnt++;
	int channels = 2;
	int copied = len;
#ifdef __PS3__
	{
		int frames_out = len / (2 * channels);
		unsigned int rd = ps3_aud_rd;
		unsigned int wr = ps3_aud_wr;
		unsigned int avail = wr - rd;
		unsigned int rate;

		unsigned int target;
		if (avail < 512u)          target = (1u << 16) * 7 / 10;
		else if (avail < 1536u)    target = (1u << 16) * 85 / 100;
		else if (avail > 12288u)   target = 2u << 16;
		else if (avail > 8192u)    target = (1u << 16) * 3 / 2;
		else                       target = 1u << 16;
		{
			int diff = (int)target - (int)ps3_aud_rate_q16;
			const int step = (int)(1u << 16) / 16;
			if (diff > step) diff = step;
			else if (diff < -step) diff = -step;
			ps3_aud_rate_q16 = (unsigned int)((int)ps3_aud_rate_q16 + diff);
		}
		rate = ps3_aud_rate_q16;

		uae_s16 out_l = ps3_aud_hold_l, out_r = ps3_aud_hold_r;
		int underrun_seen = 0;

		for (int o = 0; o < frames_out; o++) {
			if (rd < wr) {
				unsigned int i0 = rd & PS3_AUD_FIFO_MASK;
				unsigned int i1 = (rd + 1u) & PS3_AUD_FIFO_MASK;
				int l0 = ps3_aud_fifo[i0][0], r0 = ps3_aud_fifo[i0][1];
				int l1 = (rd + 1u < wr) ? ps3_aud_fifo[i1][0] : l0;
				int r1 = (rd + 1u < wr) ? ps3_aud_fifo[i1][1] : r0;
				unsigned long long f = ps3_aud_frac_q16;
				out_l = (uae_s16)((((long long)l0) * (long long)(65536u - f)
				                 + ((long long)l1) * (long long)f) >> 16);
				out_r = (uae_s16)((((long long)r0) * (long long)(65536u - f)
				                 + ((long long)r1) * (long long)f) >> 16);
				ps3_aud_hold_l = out_l;
				ps3_aud_hold_r = out_r;
				ps3_aud_frac_q16 += rate;
				while (ps3_aud_frac_q16 >= 65536u) {
					ps3_aud_frac_q16 -= 65536u;
					rd++;
				}
			} else {
				if (!underrun_seen &&
				    ps3_aud_last_push_ms != 0 &&
				    (unsigned int)(SDL_GetTicks() - ps3_aud_last_push_ms) < 3000u) {
					ps3_aud_underruns++;
					ps3_diag_audio_stall(0);
					underrun_seen = 1;
				}
				out_l = (uae_s16)(((int)ps3_aud_hold_l * 31) / 32);
				out_r = (uae_s16)(((int)ps3_aud_hold_r * 31) / 32);
				ps3_aud_hold_l = out_l;
				ps3_aud_hold_r = out_r;
			}
			((uae_s16 *)stream)[2 * o] = out_l;
			((uae_s16 *)stream)[2 * o + 1] = out_r;
		}
		ps3_aud_rd = rd;
		copied = frames_out * 2 * 2;
		if (len > copied) {
			memset(stream + copied, 0, len - copied);
		}
	}
#else
	channels = mainMenu_soundStereo ? 2 : 1;
	copied = MIN(mainMenu_soundStereo ? SNDBUFFER_LEN * 2 : SNDBUFFER_LEN, len);
	memcpy(stream, sndbuffer[cnt%SOUND_BUFFERS_COUNT], copied);
	if (len > copied) {
		memset(stream + copied, 0, len - copied);
	}
#endif
	cdrom_mix_audio((uae_s16 *)stream, copied / (channels * 2), channels, sound_rate);
	disk_sound_mix((uae_s16 *)stream, copied / (channels * 2), channels, sound_rate);
	midi_synth_mix((int16_t *)stream, copied / (channels * 2), channels, (int)sound_rate);
}

static int gp2x_start_sound(int rate, int bits, int stereo)
{
#ifdef __PS3__
	stereo = 1;
	rate = 48000;
	sound_rate = 48000;
#endif

	int frag = 0, buffers, ret;
	unsigned int bsize;
	static int audioOpened = 0;

	if (!sound_thread_active)
	{
		printf("starting sound thread..\n");
#ifdef SOUND_USE_SEMAPHORES
		uae_sem_init(&sound_sem, 0, 0);
		uae_sem_init(&callback_sem, 0, 0);
#endif
		sound_write_idx = 3;
		sound_read_idx = 0;
		memset(sndbuffer, 0, sizeof(sndbuffer));
		sndbufpt = render_sndbuff = sndbuffer[sound_write_idx % SOUND_BUFFERS_COUNT];
	}

	if (rate == s_oldrate && s_oldbits == bits && s_oldstereo == stereo)
	   return 0;

	if( audioOpened ) {
#if defined(__PSP2__)
		SDL_PauseAudio(1);
		sound_thread_exit = 1;
#ifdef SOUND_USE_SEMAPHORES
		uae_sem_post(&sound_sem);
#endif
		SDL_Delay(333);
		SDL_CloseAudio();
		sound_thread_exit=0;
#else
		return 0;
#endif
	}

	SDL_AudioSpec as;
	memset(&as, 0, sizeof(as));

	as.freq = rate;
#if UAEBIGENDIAN
	as.format = (bits == 8 ? AUDIO_S8 : AUDIO_S16SYS);
#else
	as.format = (bits == 8 ? AUDIO_S8 : AUDIO_S16);
#endif
	as.channels = (stereo ? 2 : 1);
#ifdef __PS3__
	  as.samples = SNDBUFFER_LEN*2 / as.channels / 2;
#else
	if(mainMenu_soundStereo)
	  as.samples = SNDBUFFER_LEN*2 / as.channels / 2;
	else
	  as.samples = SNDBUFFER_LEN / as.channels / 2;
#endif
	as.callback = sound_thread_mixer;
    if (SDL_OpenAudio(&as, NULL) < 0)
        return -1;
    audioOpened = 1;

	s_oldrate = rate;
	s_oldbits = bits;
	s_oldstereo = stereo;

	SDL_PauseAudio (0);

	return 0;
}


void gp2x_stop_sound(void)
{
	if (sound_thread_exit)
		printf("don't call gp2x_stop_sound more than once!\n");
	if (sound_thread_active)
	{
		printf("stopping sound thread..\n");
		sound_thread_exit = 1;
#ifdef SOUND_USE_SEMAPHORES
		uae_sem_post(&sound_sem);
#endif
	}
	SDL_PauseAudio (1);
}

void finish_sound_buffer (void)
{
#ifdef DEBUG_SOUND
	dbg("sound.c : finish_sound_buffer");
#endif

#if defined(__PS3__)
	extern int ps3_fast_forward;
	if (ps3_fast_forward) {
		sound_read_idx = sound_write_idx;
		sound_write_idx++;
		sndbufpt = render_sndbuff = sndbuffer[sound_write_idx % SOUND_BUFFERS_COUNT];
		return;
	}

	{
		const uae_u16 *push_src = sndbuffer[sound_write_idx % SOUND_BUFFERS_COUNT];
		unsigned int wr = ps3_aud_wr;
		unsigned int rd = ps3_aud_rd;
		if (wr - rd + (unsigned int)(SNDBUFFER_LEN / 2) > (unsigned int)PS3_AUD_FIFO_FRAMES) {
			rd = wr + (unsigned int)(SNDBUFFER_LEN / 2) - (unsigned int)PS3_AUD_FIFO_FRAMES;
			ps3_aud_rd = rd;
		}
		for (unsigned int i = 0; i < (unsigned int)(SNDBUFFER_LEN / 2); i++) {
			unsigned int fi = (wr + i) & PS3_AUD_FIFO_MASK;
			ps3_aud_fifo[fi][0] = (uae_s16)push_src[2 * i];
			ps3_aud_fifo[fi][1] = (uae_s16)push_src[2 * i + 1];
		}
		ps3_aud_wr = wr + (unsigned int)(SNDBUFFER_LEN / 2);
		ps3_aud_last_push_ms = SDL_GetTicks();
	}

	sound_write_idx++;
	if (sound_write_idx - sound_read_idx >= SOUND_BUFFERS_COUNT) {
		sound_read_idx = sound_write_idx - (SOUND_BUFFERS_COUNT / 2);
	}
	sndbufpt = render_sndbuff = sndbuffer[sound_write_idx % SOUND_BUFFERS_COUNT];
#else
#ifdef SOUND_USE_SEMAPHORES
	uae_sem_post(&sound_sem);
	uae_sem_wait(&callback_sem);
#endif
	static int wrcnt = 2;
	wrcnt++;
	sndbufpt = render_sndbuff = sndbuffer[wrcnt%SOUND_BUFFERS_COUNT];
#endif

#ifdef DEBUG_SOUND
	dbg(" sound.c : ! finish_sound_buffer");
#endif
}


int setup_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : setup_sound");
#endif


#ifdef DEBUG_SOUND
    dbg(" sound.c : ! setup_sound");
#endif
    return 1;
}

static int open_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : open_sound");
#endif

#ifdef __PS3__
    sound_rate = 48000;
#endif

    if (gp2x_start_sound(sound_rate, 16, mainMenu_soundStereo) != 0)
        return 0;

    disk_sound_init();
    midi_synth_init();
    sound_default_evtime();


    have_sound = 1;
    scaled_sample_evtime_ok = 1;
    sound_available = 1;

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! open_sound");
#endif
    return 1;
}

void close_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : close_sound");
#endif
    if (!have_sound)
	return;

    have_sound = 0;
    disk_sound_shutdown();

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! close_sound");
#endif
}

int init_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : init_sound");
#endif

    have_sound=open_sound();

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! init_sound");
#endif
    return have_sound;
}

void pause_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : pause_sound");
#endif

#ifndef __PS3__
	SDL_PauseAudio (1);
#endif

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! pause_sound");
#endif
}

void resume_sound (void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : resume_sound");
#endif

#ifndef __PS3__
	SDL_PauseAudio (0);
#endif

#ifdef DEBUG_SOUND
    dbg(" sound.c : ! resume_sound");
#endif
}

void uae4all_init_sound(void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : uae4all_init_sound");
#endif
#ifdef DEBUG_SOUND
    dbg(" sound.c : ! uae4all_init_sound");
#endif
}

void uae4all_pause_music(void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : pause_music");
#endif
#ifdef DEBUG_SOUND
    dbg(" sound.c : ! pause_music");
#endif
}

void uae4all_resume_music(void)
{
#ifdef DEBUG_SOUND
    dbg("sound.c : resume_music");
#endif
#ifdef DEBUG_SOUND
    dbg(" sound.c : ! resume_music");
#endif
}

void uae4all_play_click(void)
{
}
#endif

