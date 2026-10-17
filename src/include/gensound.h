 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Prototypes for general sound related functions
  * This use to be called sound.h, but that causes confusion
  *
  * Copyright 1997 Bernd Schmidt
  */

extern int sound_available;

extern unsigned long sample_evtime, scaled_sample_evtime;
extern int scaled_sample_evtime_ok;

extern int setup_sound (void);

extern int init_sound (void);
extern void close_sound (void);

extern void sample_handler (void);
extern void init_sound_table(void);

