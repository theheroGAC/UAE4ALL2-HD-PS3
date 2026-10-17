 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Call (some) Amiga Exec functions outside the main UAE thread
  * and without stack magic.
  *
  * Copyright 1999 Patrick Ohly
  *
  * Uses the EXTER interrupt that is setup in filesys.c
  * and some of it needs thread support.
  */

/*
 * The following functions do exactly the same thing as their
 * Amiga counterpart, but can be called in situation where calling
 * the exec.library functions is impossible.
 */
#ifdef SUPPORT_THREADS
void uae_ReplyMsg(uaecptr msg);
void uae_PutMsg(uaecptr port, uaecptr msg);
void uae_Signal(uaecptr task, uae_u32 mask);
#endif
void uae_NewList(uaecptr list);

uaecptr uae_AllocMem (uae_u32 size, uae_u32 flags);


void native2amiga_install (void);

void native2amiga_startup (void);

#ifdef SUPPORT_THREADS
extern smp_comm_pipe native2amiga_pending;
#endif
