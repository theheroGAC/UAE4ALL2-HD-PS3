#ifndef M68KIntrfH
#define M68KIntrfH

#include "menu_config.h"

/* defined in m68k_cmn_intrf.cpp */
extern unsigned mispcflags;

void init_memmaps(addrbank* banco);
void map_zone(unsigned addr, addrbank* banco, unsigned realstart);
void m68k_go(int may_quit);

#define M68KCONTEXT m68kcontext

#define m68k_irq_update() m68kcontext.more_cycles_to_do=m68kcontext.io_cycle_counter; m68kcontext.io_cycle_counter=0;


#include "m68k/fame/fame.h"
#if !defined(__PSP2__) && !defined(__PS3__)
#include "memory.h"
#endif

extern M68K_CONTEXT M68KCONTEXT;

#define _68k_dreg(num) (M68KCONTEXT.dreg[(num)].D)
#define _68k_areg(num) (M68KCONTEXT.areg[(num)].D)
#define _68k_sreg 	M68KCONTEXT.sr
#define _68k_ispreg 	M68KCONTEXT.isp
#define _68k_mspreg 	M68KCONTEXT.msp
#define _68k_uspreg 	M68KCONTEXT.usp
#define _68k_intmask   ((M68KCONTEXT.sr >> 8) & 7)
#define _68k_spcflags mispcflags

static __inline__ void _68k_setpc(unsigned mipc)
{
	M68KCONTEXT.pc=mipc;
	m68k_set_register(M68K_REG_PC, mipc);
}

static __inline__ void set_special (unsigned x)
{
    _68k_spcflags |= x;
}

static __inline__ void unset_special (uae_u32 x)
{
    _68k_spcflags &= ~x;
}


/* PocketUAE */

/* UAE redefine */
#define m68k_dreg(r,num) (m68kcontext.dreg[num].D)
#define m68k_areg(r,num) (m68kcontext.areg[num].D)
#define m68k_reg(num) ((num) < 8 ? _68k_dreg(num) : _68k_areg((num) - 8))
#define m68k_getpc() m68k_get_pc()
#define m68k_setpc(mipc) _68k_setpc(mipc)

#ifndef regs
#define regs M68KCONTEXT
#endif

#define GET_CFLG() ((M68KCONTEXT.flag_c >> 8) & 1)
#define GET_VFLG() ((M68KCONTEXT.flag_v >> 7) & 1)
#define GET_ZFLG() (!M68KCONTEXT.flag_notz)
#define GET_NFLG() ((M68KCONTEXT.flag_n >> 7) & 1)
#define GET_XFLG() ((M68KCONTEXT.flag_x >> 8) & 1)

#define SET_CFLG(y) (M68KCONTEXT.flag_c = ((y) ? 1 : 0) << 8)
#define SET_VFLG(y) (M68KCONTEXT.flag_v = ((y) ? 1 : 0) << 7)
#define SET_ZFLG(y) (M68KCONTEXT.flag_notz = !(y))
#define SET_NFLG(y) (M68KCONTEXT.flag_n = ((y) ? 1 : 0) << 7)
#define SET_XFLG(y) (M68KCONTEXT.flag_x = ((y) ? 1 : 0) << 8)

static __inline__ int cctrue(int cc)
{
	switch (cc & 0xF) {
		case 0:  return 1;
		case 1:  return 0;
		case 2:  return (!(M68KCONTEXT.flag_c & 0x100)) && M68KCONTEXT.flag_notz;
		case 3:  return (M68KCONTEXT.flag_c & 0x100) || (!M68KCONTEXT.flag_notz);
		case 4:  return !(M68KCONTEXT.flag_c & 0x100);
		case 5:  return (M68KCONTEXT.flag_c & 0x100) != 0;
		case 6:  return M68KCONTEXT.flag_notz != 0;
		case 7:  return !M68KCONTEXT.flag_notz;
		case 8:  return !(M68KCONTEXT.flag_v & 0x80);
		case 9:  return (M68KCONTEXT.flag_v & 0x80) != 0;
		case 10: return !(M68KCONTEXT.flag_n & 0x80);
		case 11: return (M68KCONTEXT.flag_n & 0x80) != 0;
		case 12: return !((M68KCONTEXT.flag_n ^ M68KCONTEXT.flag_v) & 0x80);
		case 13: return ((M68KCONTEXT.flag_n ^ M68KCONTEXT.flag_v) & 0x80) != 0;
		case 14: return M68KCONTEXT.flag_notz && (!((M68KCONTEXT.flag_n ^ M68KCONTEXT.flag_v) & 0x80));
		case 15: return (!M68KCONTEXT.flag_notz) || (((M68KCONTEXT.flag_n ^ M68KCONTEXT.flag_v) & 0x80) != 0);
	}
	return 0;
}

/***************/

#endif
