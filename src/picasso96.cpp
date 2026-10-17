#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "uae.h"
#include "memory-uae.h"
#include "custom.h"
#include "m68k/m68k_intrf.h"
#include "autoconf.h"
#include "xwin.h"
#include "picasso96.h"
#include "savestate.h"
#include "drawing.h"
#include <SDL.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef USE_SDL2
#include "sdl2_to_sdl1.h"
#endif

extern uae_u16 *prSDLScreenPixels;
extern char *gfx_mem;
extern unsigned gfx_rowbytes;

int picasso_on = 0;
struct picasso96_state_struct picasso96_state;
struct PicassoResolution DisplayModes[MAX_PICASSO_MODES];
static int mode_count = 0;
static uaecptr p96_CalculateBytesPerRow_addr = 0;
static uaecptr p96_SetSwitch_addr = 0;
static uaecptr p96_SetColorArray_addr = 0;
static uaecptr p96_SetDAC_addr = 0;
static uaecptr p96_SetGC_addr = 0;
static uaecptr p96_SetPanning_addr = 0;
static uaecptr p96_SetDisplay_addr = 0;
static uaecptr p96_FillRect_addr = 0;
static uaecptr p96_BlitRect_addr = 0;
static uaecptr p96_BlitRectNoMaskComplete_addr = 0;
static uaecptr p96_InvertRect_addr = 0;
static uaecptr p96_BlitTemplate_addr = 0;
static uaecptr p96_BlitPattern_addr = 0;
static uaecptr p96_BlitPlanar2Direct_addr = 0;
static uaecptr p96_BlitPlanar2Chunky_addr = 0;
static uaecptr p96_AllocCardMem_addr = 0;
static uaecptr p96_FreeCardMem_addr = 0;
static uaecptr p96_CalculateMemory_addr = 0;
static uaecptr p96_GetCompatibleFormats_addr = 0;
static uaecptr p96_ResolvePixelClock_addr = 0;
static uaecptr p96_GetPixelClock_addr = 0;
static uaecptr p96_WaitVerticalSync_addr = 0;
static uaecptr p96_UnsupportedVector_addr = 0;

static uae_u8 get_bytes_per_pixel(uae_u32 format)
{
    switch (format) {
        case RGBFB_CLUT: return 1;
        case RGBFB_R5G6B5PC:
        case RGBFB_R5G5B5PC:
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
        case RGBFB_B5G6R5PC:
        case RGBFB_B5G5R5PC: return 2;
        case RGBFB_R8G8B8:
        case RGBFB_B8G8R8: return 3;
        case RGBFB_A8R8G8B8:
        case RGBFB_A8B8G8R8:
        case RGBFB_R8G8B8A8:
        case RGBFB_B8G8R8A8: return 4;
        default: return 0;
    }
}

extern SDL_Surface *prSDLScreen;
extern int visibleAreaWidth;
extern int mainMenu_displayedLines;

static void picasso_safe_set_videomode(int w, int h);

static uae_u32 REGPARAM2 gfxmem_lget (uaecptr addr)
{
    uae_u32 *m;
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    m = (uae_u32 *)(gfxmemory + addr);
    return do_get_mem_long (m);
}

static uae_u32 REGPARAM2 gfxmem_wget (uaecptr addr)
{
    uae_u16 *m;
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    m = (uae_u16 *)(gfxmemory + addr);
    return do_get_mem_word (m);
}

static uae_u32 REGPARAM2 gfxmem_bget (uaecptr addr)
{
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    return do_get_mem_byte (gfxmemory + addr);
}

static void REGPARAM2 gfxmem_lput (uaecptr addr, uae_u32 l)
{
    uae_u32 *m;
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    m = (uae_u32 *)(gfxmemory + addr);
    do_put_mem_long (m, l);
}

static void REGPARAM2 gfxmem_wput (uaecptr addr, uae_u32 w)
{
    uae_u16 *m;
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    m = (uae_u16 *)(gfxmemory + addr);
    do_put_mem_word (m, w);
}

static void REGPARAM2 gfxmem_bput (uaecptr addr, uae_u32 b)
{
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    do_put_mem_byte (gfxmemory + addr, (uae_u8)b);
}

static int REGPARAM2 gfxmem_check (uaecptr addr, uae_u32 size)
{
    if (!gfxmemory || addr < gfxmem_start) return 0;
    uae_u32 offset = addr - gfxmem_start;
    return offset <= allocated_gfxmem && size <= allocated_gfxmem - offset;
}

static uae_u8 REGPARAM2 *gfxmem_xlate (uaecptr addr)
{
    if (!gfxmemory || addr < gfxmem_start || addr - gfxmem_start >= allocated_gfxmem)
        return NULL;
    return gfxmemory + (addr - gfxmem_start);
}

static int gfxmem_range(uaecptr addr, size_t size, uae_u32 *offset)
{
    if (!gfxmemory || !allocated_gfxmem || addr < gfxmem_start) return 0;
    uae_u32 pos = addr - gfxmem_start;
    if (pos > allocated_gfxmem || size > (size_t)allocated_gfxmem - pos) return 0;
    if (offset) *offset = pos;
    return 1;
}

static int validate_render_target(uaecptr mem, uae_s16 bytes_per_row,
                                  uae_u32 format, uae_u16 x, uae_u16 y,
                                  uae_u16 width, uae_u16 height,
                                  uae_u8 *bytes_per_pixel, uae_u32 *offset)
{
    uae_u8 bpp = get_bytes_per_pixel(format);
    if (!bpp || bytes_per_row <= 0 || !width || !height) return 0;
    if ((uae_u64)(x + width) * bpp > (uae_u64)(uae_u16)bytes_per_row) return 0;
    if (!gfxmem_range(mem, 1, offset)) return 0;
    uae_u64 base = (uae_u64)(mem - gfxmem_start);
    uae_u64 end = base
        + (uae_u64)(y + height - 1) * (uae_u16)bytes_per_row
        + (uae_u64)(x + width) * bpp;
    if (end > allocated_gfxmem) return 0;
    *bytes_per_pixel = bpp;
    return 1;
}

static void write_pixel(uae_u8 *dst, uae_u8 bpp, uae_u32 color);

static uae_u32 read_pixel(const uae_u8 *src, uae_u8 bpp)
{
    if (bpp == 1)
        return do_get_mem_byte((uae_u8 *)src);
    if (bpp == 2)
        return do_get_mem_word((uae_u16 *)src);
    if (bpp == 3)
        return (uae_u32)do_get_mem_byte((uae_u8 *)src)
             | ((uae_u32)do_get_mem_byte((uae_u8 *)src + 1) << 16)
             | ((uae_u32)do_get_mem_byte((uae_u8 *)src + 2) << 8);
    if (bpp == 4)
        return do_get_mem_long((uae_u32 *)src);
    return 0;
}

static void write_pixel(uae_u8 *dst, uae_u8 bpp, uae_u32 color)
{
    if (bpp == 1)
        do_put_mem_byte(dst, (uae_u8)color);
    else if (bpp == 2)
        do_put_mem_word((uae_u16 *)dst, (uae_u16)color);
    else if (bpp == 3) {
        do_put_mem_byte(dst, (uae_u8)color);
        do_put_mem_byte(dst + 1, (uae_u8)(color >> 16));
        do_put_mem_byte(dst + 2, (uae_u8)(color >> 8));
    } else if (bpp == 4)
        do_put_mem_long((uae_u32 *)dst, color);
}

static uae_u32 pixel_mask_for_bpp(uae_u8 bpp)
{
    return bpp == 1 ? 0xFFu : bpp == 2 ? 0xFFFFu :
           bpp == 3 ? 0xFFFFFFu : 0xFFFFFFFFu;
}

static uae_u32 p96_minterm_apply(uae_u32 minterm, uae_u32 src, uae_u32 dst,
                             uae_u32 pixel_mask)
{
    src &= pixel_mask;
    dst &= pixel_mask;
    switch (minterm & 0x0F) {
        case BLIT_FALSE:       return 0;
        case BLIT_NOR:         return ~(src | dst) & pixel_mask;
        case BLIT_ONLYDST:     return (~src & dst) & pixel_mask;
        case BLIT_NOTSRC:      return ~src & pixel_mask;
        case BLIT_ONLYSRC:     return (src & ~dst) & pixel_mask;
        case BLIT_NOTDST:      return ~dst & pixel_mask;
        case BLIT_EOR:         return (src ^ dst) & pixel_mask;
        case BLIT_NAND:        return ~(src & dst) & pixel_mask;
        case BLIT_AND:         return src & dst;
        case BLIT_NEOR:        return ~(src ^ dst) & pixel_mask;
        case BLIT_DST:         return dst;
        case BLIT_NOTONLYSRC:  return (~src | dst) & pixel_mask;
        case BLIT_SRC:         return src;
        case BLIT_NOTONLYDST:  return (src | ~dst) & pixel_mask;
        case BLIT_OR:          return (src | dst) & pixel_mask;
        case BLIT_TRUE:        return pixel_mask;
        default:               return 0;
    }
}

static void write_draw_pixel(uae_u8 *dst, uae_u8 bpp, uae_u32 fg,
                             uae_u32 bg, uae_u32 mask, uae_u8 draw_mode,
                             int bit_set)
{
    const uae_u32 full_mask = pixel_mask_for_bpp(bpp);
    uae_u32 old = read_pixel(dst, bpp);
    if (draw_mode & INVERS) {
        bit_set = !bit_set;
        draw_mode &= 0x03;
    }
    if (draw_mode == COMP) {
        uae_u32 xor_mask = bpp == 1 ? (mask & full_mask) : full_mask;
        if (bit_set)
            write_pixel(dst, bpp, old ^ (fg & xor_mask));
        return;
    }
    if (draw_mode == JAM1 && !bit_set)
        return;
    write_pixel(dst, bpp, bit_set ? fg : bg);
    if (bpp == 1 && mask != 0xFF)
        write_pixel(dst, bpp, (read_pixel(dst, bpp) & mask) | (old & ~mask));
}


#define P96_MAX_MEMORY_CHUNKS 256
struct p96_memory_chunk {
    uaecptr address;
    uae_u32 size;
    int used;
};
static struct p96_memory_chunk p96_memory_chunks[P96_MAX_MEMORY_CHUNKS];
static struct p96_memory_chunk picasso96_restore_chunks[P96_MAX_MEMORY_CHUNKS];
static struct picasso96_state_struct picasso96_restore_state_data;
static uae_u32 picasso96_restore_gfxmem_size;
static long picasso96_restore_vram_filepos;
static uae_u32 picasso96_restore_vram_len;
static int picasso96_restore_pending;
static int picasso96_restore_vram_pending;

static void p96_save_u32(uae_u8 **dst, uae_u32 value)
{
    (*dst)[0] = (uae_u8)(value >> 24);
    (*dst)[1] = (uae_u8)(value >> 16);
    (*dst)[2] = (uae_u8)(value >> 8);
    (*dst)[3] = (uae_u8)value;
    *dst += 4;
}

static uae_u32 p96_restore_u32(const uae_u8 **src, const uae_u8 *end)
{
    uae_u32 value;
    if (!src || !*src || !end || end - *src < 4)
        return 0;
    value = ((uae_u32)(*src)[0] << 24) | ((uae_u32)(*src)[1] << 16)
          | ((uae_u32)(*src)[2] << 8) | (uae_u32)(*src)[3];
    *src += 4;
    return value;
}

void picasso_memory_reset(void)
{
    const int restoring = (savestate_state == STATE_RESTORE);
    memset(p96_memory_chunks, 0, sizeof(p96_memory_chunks));
    memset(&picasso96_state, 0, sizeof(struct picasso96_state_struct));
    picasso_on = 0;
    gfxmem_start = 0;
    gfxmem_mask = 0;
    gfxmem_bank.baseaddr = NULL;
    if (!restoring) {
        memset(picasso96_restore_chunks, 0, sizeof(picasso96_restore_chunks));
        memset(&picasso96_restore_state_data, 0, sizeof(picasso96_restore_state_data));
        picasso96_restore_gfxmem_size = 0;
        picasso96_restore_vram_filepos = 0;
        picasso96_restore_vram_len = 0;
        picasso96_restore_pending = 0;
        picasso96_restore_vram_pending = 0;
    }
}

void picasso_reset(void)
{
    if (gfxmemory && allocated_gfxmem)
        memset(gfxmemory, 0, allocated_gfxmem);
    picasso_memory_reset();
}

void picasso_shutdown(void)
{
    picasso_on = 0;
    picasso96_state.SwitchState = 0;
}

void picasso_restore_begin(void)
{
    picasso_on = 0;
    memset(&picasso96_state, 0, sizeof(struct picasso96_state_struct));
    memset(p96_memory_chunks, 0, sizeof(p96_memory_chunks));
    memset(picasso96_restore_chunks, 0, sizeof(picasso96_restore_chunks));
    if (gfxmemory && allocated_gfxmem)
        memset(gfxmemory, 0, allocated_gfxmem);
    picasso96_restore_pending = 0;
    picasso96_restore_vram_pending = 0;
}

void picasso_restore_pending(void)
{
    picasso96_restore_pending = 1;
}

uae_u8 *picasso_save_state(int *len)
{
    const uae_u32 size = 19 * 4 + 256 * 4 + 4 + P96_MAX_MEMORY_CHUNKS * 12;
    uae_u8 *buffer;
    uae_u8 *dst;
    uae_u32 address_offset = 0;
    uae_u32 extent_offset = 0;

    if (!len)
        return NULL;
    *len = 0;
    buffer = (uae_u8 *)malloc(size);
    if (!buffer)
        return NULL;
    dst = buffer;
    p96_save_u32(&dst, 0x50393653);
    p96_save_u32(&dst, 2);
    p96_save_u32(&dst, allocated_gfxmem);
    p96_save_u32(&dst, (uae_u32)picasso_on);
    p96_save_u32(&dst, (uae_u32)picasso96_state.SwitchState);
    p96_save_u32(&dst, (uae_u32)picasso96_state.CardFound);
    p96_save_u32(&dst, picasso96_state.RGBFormat);
    if (picasso96_state.Address >= gfxmem_start && gfxmem_start
        && picasso96_state.Address - gfxmem_start < allocated_gfxmem)
        address_offset = picasso96_state.Address - gfxmem_start;
    if (picasso96_state.Extent >= gfxmem_start && gfxmem_start
        && picasso96_state.Extent - gfxmem_start <= allocated_gfxmem)
        extent_offset = picasso96_state.Extent - gfxmem_start;
    p96_save_u32(&dst, address_offset);
    p96_save_u32(&dst, extent_offset);
    p96_save_u32(&dst, picasso96_state.Width);
    p96_save_u32(&dst, picasso96_state.VirtualWidth);
    p96_save_u32(&dst, picasso96_state.BytesPerRow);
    p96_save_u32(&dst, picasso96_state.Height);
    p96_save_u32(&dst, picasso96_state.VirtualHeight);
    p96_save_u32(&dst, picasso96_state.GC_Depth);
    p96_save_u32(&dst, picasso96_state.GC_Flags);
    p96_save_u32(&dst, (uae_u32)(uae_s32)picasso96_state.XOffset);
    p96_save_u32(&dst, (uae_u32)(uae_s32)picasso96_state.YOffset);
    p96_save_u32(&dst, picasso96_state.BytesPerPixel);
    for (int i = 0; i < 256; i++) {
        *dst++ = picasso96_state.CLUT[i].Red;
        *dst++ = picasso96_state.CLUT[i].Green;
        *dst++ = picasso96_state.CLUT[i].Blue;
        *dst++ = picasso96_state.CLUT[i].Pad;
    }
    p96_save_u32(&dst, P96_MAX_MEMORY_CHUNKS);
    for (int i = 0; i < P96_MAX_MEMORY_CHUNKS; i++) {
        uae_u32 offset = 0;
        if (p96_memory_chunks[i].used && p96_memory_chunks[i].address >= gfxmem_start
            && p96_memory_chunks[i].address - gfxmem_start < allocated_gfxmem)
            offset = p96_memory_chunks[i].address - gfxmem_start;
        p96_save_u32(&dst, offset);
        p96_save_u32(&dst, p96_memory_chunks[i].size);
        p96_save_u32(&dst, p96_memory_chunks[i].used ? 1 : 0);
    }
    *len = (int)(dst - buffer);
    return buffer;
}

uae_u8 *picasso_save_vram(int *len)
{
    if (!len)
        return NULL;
    *len = (gfxmemory && allocated_gfxmem) ? (int)allocated_gfxmem : 0;
    return gfxmemory;
}

void picasso_restore_state(uae_u8 *data, int len)
{
    const uae_u8 *src = data;
    const uae_u8 *end = data ? data + len : NULL;
    uae_u32 magic;
    uae_u32 version;

    if (!data || len < 16)
        return;
    magic = p96_restore_u32(&src, end);
    version = p96_restore_u32(&src, end);
    if (magic != 0x50393653 || (version != 1 && version != 2))
        return;

    memset(&picasso96_restore_state_data, 0, sizeof(picasso96_restore_state_data));
    memset(picasso96_restore_chunks, 0, sizeof(picasso96_restore_chunks));
    picasso96_restore_gfxmem_size = p96_restore_u32(&src, end);
    (void)p96_restore_u32(&src, end);
    picasso96_restore_state_data.SwitchState = (uae_u8)p96_restore_u32(&src, end);
    picasso96_restore_state_data.CardFound = (uae_u8)p96_restore_u32(&src, end);
    picasso96_restore_state_data.RGBFormat = p96_restore_u32(&src, end);
    picasso96_restore_state_data.Address = p96_restore_u32(&src, end);
    picasso96_restore_state_data.Extent = p96_restore_u32(&src, end);
    picasso96_restore_state_data.Width = (uae_u16)p96_restore_u32(&src, end);
    picasso96_restore_state_data.VirtualWidth = (uae_u16)p96_restore_u32(&src, end);
    picasso96_restore_state_data.BytesPerRow = (uae_u16)p96_restore_u32(&src, end);
    picasso96_restore_state_data.Height = (uae_u16)p96_restore_u32(&src, end);
    picasso96_restore_state_data.VirtualHeight = (uae_u16)p96_restore_u32(&src, end);
    picasso96_restore_state_data.GC_Depth = (uae_u8)p96_restore_u32(&src, end);
    picasso96_restore_state_data.GC_Flags = (uae_u8)p96_restore_u32(&src, end);
    picasso96_restore_state_data.XOffset = (uae_s32)p96_restore_u32(&src, end);
    picasso96_restore_state_data.YOffset = (uae_s32)p96_restore_u32(&src, end);
    picasso96_restore_state_data.BytesPerPixel = (uae_u8)p96_restore_u32(&src, end);
    if (!src || !end || end - src < 256 * 4)
        return;
    for (int i = 0; i < 256; i++) {
        picasso96_restore_state_data.CLUT[i].Red = *src++;
        picasso96_restore_state_data.CLUT[i].Green = *src++;
        picasso96_restore_state_data.CLUT[i].Blue = *src++;
        picasso96_restore_state_data.CLUT[i].Pad = *src++;
    }
    if (version >= 2) {
        uae_u32 chunk_count = p96_restore_u32(&src, end);
        if (chunk_count > P96_MAX_MEMORY_CHUNKS)
            chunk_count = P96_MAX_MEMORY_CHUNKS;
        for (uae_u32 i = 0; i < chunk_count; i++) {
            picasso96_restore_chunks[i].address = p96_restore_u32(&src, end);
            picasso96_restore_chunks[i].size = p96_restore_u32(&src, end);
            picasso96_restore_chunks[i].used = p96_restore_u32(&src, end) ? 1 : 0;
        }
    }
    if (!src || !end || src > end)
        return;
    if (picasso96_restore_gfxmem_size == 0 || picasso96_restore_gfxmem_size > 0x40000000)
        return;
    changed_prefs.gfxmem_size = picasso96_restore_gfxmem_size;
    picasso96_restore_pending = 1;
}

void picasso_restore_vram(long filepos, uae_u32 len)
{
    picasso96_restore_vram_filepos = filepos;
    picasso96_restore_vram_len = len;
    picasso96_restore_vram_pending = (filepos >= 0 && len != 0);
}

void picasso_restore_after_mapping(void)
{
    if (!picasso96_restore_pending || !gfxmemory || !allocated_gfxmem) {
        if (picasso96_restore_pending && !gfxmemory)
            write_log("P96: restore skipped because VRAM is unavailable\n");
        return;
    }
    picasso96_state = picasso96_restore_state_data;
    memset(p96_memory_chunks, 0, sizeof(p96_memory_chunks));
    for (int i = 0; i < P96_MAX_MEMORY_CHUNKS; i++) {
        if (!picasso96_restore_chunks[i].used || !picasso96_restore_chunks[i].size)
            continue;
        if (picasso96_restore_chunks[i].address >= allocated_gfxmem
            || picasso96_restore_chunks[i].size > allocated_gfxmem - picasso96_restore_chunks[i].address)
            continue;
        p96_memory_chunks[i] = picasso96_restore_chunks[i];
        p96_memory_chunks[i].address += gfxmem_start;
    }
    if (picasso96_state.Address < allocated_gfxmem)
        picasso96_state.Address += gfxmem_start;
    else
        picasso96_state.Address = 0;
    if (picasso96_state.Extent <= allocated_gfxmem)
        picasso96_state.Extent += gfxmem_start;
    else
        picasso96_state.Extent = 0;
    picasso_on = picasso96_state.SwitchState ? 1 : 0;
    if (picasso_on && picasso96_state.Width > 0 && picasso96_state.Height > 0)
        picasso_safe_set_videomode(picasso96_state.Width, picasso96_state.Height);
    if (picasso96_restore_vram_pending && savestate_file) {
        if (fseek(savestate_file, picasso96_restore_vram_filepos, SEEK_SET) == 0) {
            uae_u32 copy_len = picasso96_restore_vram_len < allocated_gfxmem
                           ? picasso96_restore_vram_len : allocated_gfxmem;
            if (fread(gfxmemory, 1, copy_len, savestate_file) != copy_len)
                write_log("P96: short VRAM restore (%lu bytes)\n", (unsigned long)copy_len);
            if (copy_len < allocated_gfxmem)
                memset(gfxmemory + copy_len, 0, allocated_gfxmem - copy_len);
        } else {
            write_log("P96: unable to seek to VRAM snapshot\n");
        }
    }
    picasso96_restore_pending = 0;
    picasso96_restore_vram_pending = 0;
    picasso_refresh();
}

static uae_u32 p96_memory_limit(void)
{
    return allocated_gfxmem > 32768 ? allocated_gfxmem - 32768 : 0;
}

static uae_u32 p96_align16(uae_u32 value)
{
    return (value + 15u) & ~15u;
}

static uae_u32 p96_AllocCardMem(void)
{
    uae_u32 requested = _68k_dreg(0);
    uae_u32 size;
    uae_u32 candidate = 0;
    int slot = -1;
    int restart;

    if (!requested || requested > 0xFFFFFFF0u || !gfxmemory)
        return 0;
    size = p96_align16(requested);
    if (!size || size > p96_memory_limit())
        return 0;

    for (int i = 0; i < P96_MAX_MEMORY_CHUNKS; i++) {
        if (!p96_memory_chunks[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return 0;

    do {
        restart = 0;
        candidate = p96_align16(candidate);
        if (candidate > p96_memory_limit() || size > p96_memory_limit() - candidate)
            return 0;
        for (int i = 0; i < P96_MAX_MEMORY_CHUNKS; i++) {
            if (!p96_memory_chunks[i].used)
                continue;
            uae_u32 other = p96_memory_chunks[i].address - gfxmem_start;
            if (candidate < other + p96_memory_chunks[i].size
                && other < candidate + size) {
                if (other > 0xFFFFFFFFu - p96_memory_chunks[i].size)
                    return 0;
                candidate = other + p96_memory_chunks[i].size;
                restart = 1;
                break;
            }
        }
    } while (restart);

    p96_memory_chunks[slot].address = gfxmem_start + candidate;
    p96_memory_chunks[slot].size = size;
    p96_memory_chunks[slot].used = 1;
    return p96_memory_chunks[slot].address;
}

static uae_u32 p96_FreeCardMem(void)
{
    uaecptr address = _68k_areg(1);
    if (!address || address < gfxmem_start || !gfxmem_range(address, 1, NULL))
        return 0;
    for (int i = 0; i < P96_MAX_MEMORY_CHUNKS; i++) {
        if (p96_memory_chunks[i].used && p96_memory_chunks[i].address == address) {
            p96_memory_chunks[i].used = 0;
            p96_memory_chunks[i].address = 0;
            p96_memory_chunks[i].size = 0;
            return 1;
        }
    }
    return 0;
}

static uae_u32 p96_CalculateMemory(void)
{
    uaecptr address = _68k_areg(1);
    if (!address || !gfxmem_range(address, 1, NULL)) return 0;
    return address;
}

static uae_u32 p96_GetCompatibleFormats(void)
{
    return RGBMASK_8BIT | RGBMASK_15BIT | RGBMASK_16BIT |
           RGBMASK_24BIT | RGBMASK_32BIT;
}

addrbank gfxmem_bank = {
    gfxmem_lget, gfxmem_wget, gfxmem_bget,
    gfxmem_lput, gfxmem_wput, gfxmem_bput,
    gfxmem_xlate, gfxmem_check, NULL
};

static void add_mode(int w, int h, int d, int hz, const char *name)
{
    if (mode_count >= MAX_PICASSO_MODES) return;
    DisplayModes[mode_count].res.width = w;
    DisplayModes[mode_count].res.height = h;
    DisplayModes[mode_count].depth = d;
    DisplayModes[mode_count].refresh = hz;
    strncpy(DisplayModes[mode_count].name, name, sizeof(DisplayModes[mode_count].name) - 1);
    DisplayModes[mode_count].name[sizeof(DisplayModes[mode_count].name) - 1] = '\0';
    mode_count++;
}

void InitPicasso96(void)
{
    static int init_done = 0;
    memset(&picasso96_state, 0, sizeof(struct picasso96_state_struct));
    picasso_on = 0;
    if (init_done) return;
    init_done = 1;
    mode_count = 0;

    add_mode(320, 200, 1, 60, "320x200, 8-bit, 60Hz");
    add_mode(320, 200, 2, 60, "320x200, 16-bit, 60Hz");
    add_mode(320, 200, 4, 60, "320x200, 32-bit, 60Hz");

    add_mode(320, 240, 1, 60, "320x240, 8-bit, 60Hz");
    add_mode(320, 240, 2, 60, "320x240, 16-bit, 60Hz");
    add_mode(320, 240, 4, 60, "320x240, 32-bit, 60Hz");

    add_mode(640, 400, 1, 60, "640x400, 8-bit, 60Hz");
    add_mode(640, 400, 2, 60, "640x400, 16-bit, 60Hz");
    add_mode(640, 400, 4, 60, "640x400, 32-bit, 60Hz");

    add_mode(640, 480, 1, 60, "640x480, 8-bit, 60Hz");
    add_mode(640, 480, 2, 60, "640x480, 16-bit, 60Hz");
    add_mode(640, 480, 4, 60, "640x480, 32-bit, 60Hz");

    add_mode(800, 600, 1, 60, "800x600, 8-bit, 60Hz");
    add_mode(800, 600, 2, 60, "800x600, 16-bit, 60Hz");
    add_mode(800, 600, 4, 60, "800x600, 32-bit, 60Hz");

    add_mode(1024, 768, 1, 60, "1024x768, 8-bit, 60Hz");
    add_mode(1024, 768, 2, 60, "1024x768, 16-bit, 60Hz");

    add_mode(1280, 720, 1, 60, "1280x720, 8-bit, 60Hz");
    add_mode(1280, 720, 2, 60, "1280x720, 16-bit, 60Hz");
}

static int get_board_info_size(void)
{
    return PSSO_BoardInfo_BitMapList + 4;
}

static uaecptr get_board_info(uaecptr preferred_reg)
{
    if (preferred_reg && valid_address(preferred_reg, get_board_info_size())) return preferred_reg;
    uaecptr sp = _68k_areg(7);
    uaecptr bi_stack = sp ? get_long(sp + 8) : 0;
    if (bi_stack && valid_address(bi_stack, get_board_info_size())) return bi_stack;
    if (_68k_areg(0) && valid_address(_68k_areg(0), get_board_info_size())) return _68k_areg(0);
    if (_68k_areg(2) && valid_address(_68k_areg(2), get_board_info_size())) return _68k_areg(2);
    return 0;
}

static void picasso_safe_set_videomode(int w, int h)
{
    if (!prSDLScreen || w <= 0 || h <= 0) return;
    if (prSDLScreen->w == w && prSDLScreen->h == h) return;

    if (SDL_MUSTLOCK(prSDLScreen)) {
        while (prSDLScreen->locked) {
            SDL_UnlockSurface(prSDLScreen);
        }
    }

    SDL_FreeSurface(prSDLScreen);
    prSDLScreen = SDL_SetVideoMode(w, h, 16, SDL_HWSURFACE | SDL_DOUBLEBUF);
    if (!prSDLScreen) return;

    prSDLScreenPixels = (uae_u16 *)prSDLScreen->pixels;
    gfx_mem = (char *)prSDLScreen->pixels;
    gfx_rowbytes = prSDLScreen->pitch;
    init_row_map();
    notice_screen_contents_lost();
    notice_new_xcolors();

#if defined(__PS3__)
    SDL_LockSurface(prSDLScreen);
#endif
#ifdef USE_SDL2
    SDL_SetVideoModeScaling(0, 0, 0, 0);
#endif
}

uae_u32 picasso_FindCard(void)
{
    uaecptr AmigaBoardInfo = get_board_info(_68k_areg(0));
    if (!AmigaBoardInfo || !allocated_gfxmem || !gfxmem_start) return 0;
    put_long(AmigaBoardInfo + PSSO_BoardInfo_MemoryBase, gfxmem_start);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_MemorySize, p96_memory_limit());
    picasso96_state.CardFound = 1;
    return 1;
}

static void setup_p96_funcs(uaecptr AmigaBoardInfo)
{
    if (!p96_CalculateBytesPerRow_addr) return;
    put_long(AmigaBoardInfo + PSSO_BoardInfo_CalculateBytesPerRow, p96_CalculateBytesPerRow_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetSwitch, p96_SetSwitch_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetColorArray, p96_SetColorArray_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetDAC, p96_SetDAC_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetGC, p96_SetGC_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetPanning, p96_SetPanning_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetDisplay, p96_SetDisplay_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_AllocCardMem, p96_AllocCardMem_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_FreeCardMem, p96_FreeCardMem_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_FillRect, p96_FillRect_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_BlitRect, p96_BlitRect_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_BlitRectNoMaskComplete, p96_BlitRectNoMaskComplete_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_InvertRect, p96_InvertRect_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_BlitTemplate, p96_BlitTemplate_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_BlitPattern, p96_BlitPattern_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_BlitPlanar2Direct, p96_BlitPlanar2Direct_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_BlitPlanar2Chunky, p96_BlitPlanar2Chunky_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_CalculateMemory, p96_CalculateMemory_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_GetCompatibleFormats, p96_GetCompatibleFormats_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_ResolvePixelClock, p96_ResolvePixelClock_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_GetPixelClock, p96_GetPixelClock_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_WaitVerticalSync, p96_WaitVerticalSync_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_DrawLine, p96_UnsupportedVector_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_WaitBlitter, p96_UnsupportedVector_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetMemoryMode, p96_UnsupportedVector_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetClock, p96_UnsupportedVector_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetWriteMask, p96_UnsupportedVector_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetClearMask, p96_UnsupportedVector_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_SetReadPlane, p96_UnsupportedVector_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_ScrollPlanar, p96_UnsupportedVector_addr);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_UpdatePlanar, p96_UnsupportedVector_addr);
}

static uae_u32 AssignModeID(int w, int h, int count)
{
    if (w == 320 && h == 200) return 0x50001000;
    else if (w == 320 && h == 240) return 0x50011000;
    else if (w == 640 && h == 400) return 0x50021000;
    else if (w == 640 && h == 480) return 0x50031000;
    else if (w == 800 && h == 600) return 0x50041000;
    else if (w == 1024 && h == 768) return 0x50051000;
    else if (w == 1152 && h == 864) return 0x50061000;
    else if (w == 1280 && h == 1024) return 0x50071000;
    else if (w == 1600 && h == 1280) return 0x50081000;
    else if (w == 1280 && h == 720) return 0x500A1000;
    return 0x50101000 + (uae_u32)count * 0x10000;
}

uae_u32 picasso_InitCard(void)
{
    uaecptr AmigaBoardInfo = get_board_info(_68k_areg(2));
    if (!AmigaBoardInfo || !allocated_gfxmem || !gfxmem_start) return 0;

    put_long(AmigaBoardInfo + PSSO_BoardInfo_MemoryBase, gfxmem_start);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_MemoryIOBase, gfxmem_start);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_MemorySize, p96_memory_limit());

    uaecptr board_name_addr = gfxmem_start + allocated_gfxmem - 128;
    const char *bname = "uaegfx";
    for (size_t k = 0; k < strlen(bname); k++) put_byte(board_name_addr + k, bname[k]);
    put_byte(board_name_addr + strlen(bname), 0);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_BoardName, board_name_addr);

    for (int k = 0; k < 31; k++) {
        char c = (k < (int)strlen(bname)) ? bname[k] : 0;
        put_byte(AmigaBoardInfo + PSSO_BoardInfo_VBIName + k, c);
    }
    put_byte(AmigaBoardInfo + PSSO_BoardInfo_VBIName + 31, 0);

    put_word(AmigaBoardInfo + PSSO_BoardInfo_BitsPerCannon, 8);
    put_word(AmigaBoardInfo + PSSO_BoardInfo_RGBFormats,
             RGBMASK_8BIT | RGBMASK_15BIT | RGBMASK_16BIT | RGBMASK_24BIT | RGBMASK_32BIT);
    put_word(AmigaBoardInfo + PSSO_BoardInfo_SoftSpriteFlags,
             RGBMASK_8BIT | RGBMASK_15BIT | RGBMASK_16BIT | RGBMASK_24BIT | RGBMASK_32BIT);
    put_long(AmigaBoardInfo + PSSO_BoardInfo_BoardType, BT_uaegfx);

    for (int m = 0; m < MAXMODES; m++) {
        put_word(AmigaBoardInfo + PSSO_BoardInfo_MaxHorValue + m * 2, 1920);
        put_word(AmigaBoardInfo + PSSO_BoardInfo_MaxVerValue + m * 2, 1080);
        put_word(AmigaBoardInfo + PSSO_BoardInfo_MaxHorResolution + m * 2, 1280);
        put_word(AmigaBoardInfo + PSSO_BoardInfo_MaxVerResolution + m * 2, 720);
    }
    put_long(AmigaBoardInfo + PSSO_BoardInfo_MaxMemorySize, p96_memory_limit());
    put_long(AmigaBoardInfo + PSSO_BoardInfo_MaxChunkSize, p96_memory_limit());

    setup_p96_funcs(AmigaBoardInfo);

    uaecptr res_list_head = AmigaBoardInfo + PSSO_BoardInfo_ResolutionsList;
    put_long(res_list_head + 0, res_list_head + 4);
    put_long(res_list_head + 4, 0);
    put_long(res_list_head + 8, res_list_head);

    uaecptr mode_alloc_ptr = gfxmem_start + allocated_gfxmem - 16384;
    uaecptr res_alloc_ptr = gfxmem_start + allocated_gfxmem - 32768;
    uaecptr strings_alloc_ptr = gfxmem_start + allocated_gfxmem - 2048;

    int i = 0;
    int res_count = 0;
    while (i < mode_count) {
        int cur_w = DisplayModes[i].res.width;
        int cur_h = DisplayModes[i].res.height;
        uaecptr cur_res = res_alloc_ptr;
        res_alloc_ptr += PSSO_LibResolution_sizeof;

        for (int b = 0; b < PSSO_LibResolution_sizeof; b++)
            put_byte(cur_res + b, 0);

        for (int k = 0; k < 6; k++) put_byte(cur_res + PSSO_LibResolution_P96ID + k, "P96-0:"[k]);
        char rname[MAXRESOLUTIONNAMELENGTH];
        snprintf(rname, sizeof(rname), "%dx%d", cur_w, cur_h);
        char display_name[MAXRESOLUTIONNAMELENGTH];
        snprintf(display_name, sizeof(display_name), "uaegfx:%s", rname);
        for (size_t k = 0; k < MAXRESOLUTIONNAMELENGTH; k++) {
            char c = (k < strlen(display_name)) ? display_name[k] : 0;
            put_byte(cur_res + PSSO_LibResolution_Name + k, c);
        }
        put_long(cur_res + 10, cur_res + PSSO_LibResolution_P96ID);
        put_long(cur_res + PSSO_LibResolution_DisplayID, AssignModeID(cur_w, cur_h, res_count++));
        put_word(cur_res + PSSO_LibResolution_Width, cur_w);
        put_word(cur_res + PSSO_LibResolution_Height, cur_h);
        put_word(cur_res + PSSO_LibResolution_Flags, P96F_PUBLIC);
        for (int m = 0; m < MAXMODES; m++) put_long(cur_res + PSSO_LibResolution_Modes + m * 4, 0);
        put_long(cur_res + PSSO_LibResolution_BoardInfo, AmigaBoardInfo);

        while (i < mode_count && DisplayModes[i].res.width == cur_w && DisplayModes[i].res.height == cur_h) {
            long req = (long)DisplayModes[i].res.width * (long)DisplayModes[i].res.height * (long)DisplayModes[i].depth;
            if ((long)(allocated_gfxmem - 32768) > req) {
                uaecptr cur_mode = mode_alloc_ptr;
                mode_alloc_ptr += PSSO_ModeInfo_sizeof;
                for (int b = 0; b < PSSO_ModeInfo_sizeof; b++)
                    put_byte(cur_mode + b, 0);

                int mode_slot = CHUNKY;
                const char *depth_name = "8-bit";
                if (DisplayModes[i].depth == 2) { mode_slot = HICOLOR; depth_name = "16-bit"; }
                else if (DisplayModes[i].depth == 3) { mode_slot = TRUECOLOR; depth_name = "24-bit"; }
                else if (DisplayModes[i].depth >= 4) { mode_slot = TRUEALPHA; depth_name = "32-bit"; }
                put_long(cur_res + PSSO_LibResolution_Modes + mode_slot * 4, cur_mode);

                uaecptr mode_name_addr = strings_alloc_ptr;
                strings_alloc_ptr += 32;
                for (size_t k = 0; k < strlen(depth_name); k++) put_byte(mode_name_addr + k, depth_name[k]);
                put_byte(mode_name_addr + strlen(depth_name), 0);

                put_long(cur_mode + 10, mode_name_addr);
                put_word(cur_mode + PSSO_ModeInfo_Width, cur_w);
                put_word(cur_mode + PSSO_ModeInfo_Height, cur_h);
                put_byte(cur_mode + PSSO_ModeInfo_Depth, DisplayModes[i].depth * 8);
                put_byte(cur_mode + PSSO_ModeInfo_Flags, 0);
                put_word(cur_mode + PSSO_ModeInfo_HorTotal, cur_w);
                put_word(cur_mode + PSSO_ModeInfo_VerTotal, cur_h);
                put_long(cur_mode + PSSO_ModeInfo_PixelClock, (uae_u32)cur_w * (uae_u32)cur_h * (uae_u32)DisplayModes[i].refresh);
            }
            i++;
        }

        uaecptr pred = get_long(res_list_head + 8);
        put_long(cur_res + 0, 0);
        if (pred == res_list_head) {
            put_long(cur_res + 4, 0);
            put_long(res_list_head + 0, cur_res);
        } else {
            put_long(cur_res + 4, pred);
            put_long(pred + 0, cur_res);
        }
        put_long(res_list_head + 8, cur_res);
    }

    return 0;
}

uae_u32 picasso_SetSwitch(void)
{
    uae_u16 flag = _68k_dreg(0) & 0xFFFF;
    picasso_on = (flag != 0) ? 1 : 0;
    picasso96_state.SwitchState = picasso_on;
    if (picasso_on && picasso96_state.Width > 0 && picasso96_state.Height > 0) {
        picasso_safe_set_videomode(picasso96_state.Width, picasso96_state.Height);
    } else if (!picasso_on) {
        picasso_safe_set_videomode(visibleAreaWidth, mainMenu_displayedLines);
    }
    return !flag;
}

uae_u32 picasso_SetDisplay(void)
{
    return !(_68k_dreg(0) & 0xFFFF);
}

uae_u32 picasso_SetPanning(void)
{
    uaecptr board = _68k_areg(0);
    uaecptr screen = _68k_areg(1);
    uae_u16 width = (uae_u16)_68k_dreg(0);
    if (!board || !screen || !gfxmemory || width == 0) return 0;
    if (!gfxmem_range(screen, 1, NULL)) return 0;

    uae_u32 format = _68k_dreg(7);
    uae_u8 bpp = get_bytes_per_pixel(format);
    if (!bpp) return 0;

    uae_s16 xoff = (uae_s16)_68k_dreg(1);
    uae_s16 yoff = (uae_s16)_68k_dreg(2);
    if (xoff < 0 || yoff < 0) return 0;

    uaecptr extra = 0;
    if (valid_address(board, PSSO_BoardInfo_BitMapExtra + 4))
        extra = get_long(board + PSSO_BoardInfo_BitMapExtra);
    uae_u16 virtual_width = width;
    uae_u16 virtual_height = picasso96_state.Height;
    if (extra && valid_address(extra, PSSO_BitMapExtra_Last)) {
        uae_u16 extra_width = (uae_u16)get_word(extra + PSSO_BitMapExtra_Width);
        uae_u16 extra_height = (uae_u16)get_word(extra + PSSO_BitMapExtra_Height);
        if (extra_width >= width && extra_width != 0)
            virtual_width = extra_width;
        if (extra_height >= picasso96_state.Height && extra_height != 0)
            virtual_height = extra_height;
    }
    if (!virtual_width) return 0;
    if ((uae_u32)xoff + width > virtual_width)
        virtual_width = (uae_u16)((uae_u32)xoff + width);
    if (picasso96_state.Height && (uae_u32)yoff + picasso96_state.Height > virtual_height)
        virtual_height = (uae_u16)((uae_u32)yoff + picasso96_state.Height);

    uae_u32 bytes_per_row = (uae_u32)width * bpp;
    if (picasso96_state.Height) {
        uae_u64 required = (uae_u64)yoff * bytes_per_row
                         + (uae_u64)(picasso96_state.Height - 1) * bytes_per_row
                         + (uae_u64)width * bpp;
        if (required > 0xFFFFFFFFu || !gfxmem_range(screen, (size_t)required, NULL)) {
            write_log("P96: SetPanning rejected VRAM range screen=%08lx width=%u height=%u bpr=%lu vram=%lu\\n",
                      (unsigned long)screen, width, picasso96_state.Height,
                      (unsigned long)bytes_per_row, (unsigned long)allocated_gfxmem);
            return 0;
        }
    }

    picasso96_state.Address = screen;
    picasso96_state.XOffset = xoff;
    picasso96_state.YOffset = yoff;
    picasso96_state.RGBFormat = format;
    picasso96_state.BytesPerPixel = bpp;
    picasso96_state.VirtualWidth = virtual_width;
    picasso96_state.VirtualHeight = virtual_height;
    picasso96_state.BytesPerRow = (uae_u16)bytes_per_row;
    picasso96_state.Extent = virtual_height
        ? screen + (uae_u32)bytes_per_row * virtual_height : 0;
    return 1;
}

uae_u32 picasso_CalculateBytesPerRow(void)
{
    uae_u16 width = _68k_dreg(0) & 0xFFFF;
    uae_u32 format = _68k_dreg(7);
    uae_u32 bpp = 1;
    switch (format) {
        case RGBFB_CLUT: bpp = 1; break;
        case RGBFB_R5G6B5PC:
        case RGBFB_R5G5B5PC:
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
        case RGBFB_B5G6R5PC:
        case RGBFB_B5G5R5PC: bpp = 2; break;
        case RGBFB_R8G8B8:
        case RGBFB_B8G8R8: bpp = 3; break;
        case RGBFB_A8R8G8B8:
        case RGBFB_A8B8G8R8:
        case RGBFB_R8G8B8A8:
        case RGBFB_B8G8R8A8: bpp = 4; break;
        default: bpp = 1; break;
    }
    return width * bpp;
}

uae_u32 picasso_SetColorArray(void)
{
    uaecptr AmigaBoardInfo = _68k_areg(0);
    uae_u16 start = _68k_dreg(0) & 0xFFFF;
    uae_u16 count = _68k_dreg(1) & 0xFFFF;
    if (start >= 256 || !AmigaBoardInfo
        || !valid_address(AmigaBoardInfo, get_board_info_size())) return 0;
    if (count > 256 - start) count = 256 - start;
    uaecptr clut_ptr = AmigaBoardInfo + PSSO_BoardInfo_CLUT + start * 3;
    for (int i = 0; i < count; i++) {
        picasso96_state.CLUT[start + i].Red = get_byte(clut_ptr + i * 3 + 0);
        picasso96_state.CLUT[start + i].Green = get_byte(clut_ptr + i * 3 + 1);
        picasso96_state.CLUT[start + i].Blue = get_byte(clut_ptr + i * 3 + 2);
    }
    return 1;
}

uae_u32 picasso_SetDAC(void)
{
    return 1;
}

uae_u32 picasso_SetGC(void)
{
    uaecptr AmigaModeInfo = _68k_areg(1);
    if (!_68k_areg(0) || !AmigaModeInfo) return 0;
    picasso96_state.Width = get_word(AmigaModeInfo + PSSO_ModeInfo_Width);
    picasso96_state.Height = get_word(AmigaModeInfo + PSSO_ModeInfo_Height);
    picasso96_state.GC_Depth = get_byte(AmigaModeInfo + PSSO_ModeInfo_Depth);
    if (picasso_on && picasso96_state.Width > 0 && picasso96_state.Height > 0) {
        picasso_safe_set_videomode(picasso96_state.Width, picasso96_state.Height);
    }
    return 1;
}

uae_u32 picasso_FillRect(void)
{
    uaecptr ri_ptr = _68k_areg(1);
    if (!ri_ptr || !gfxmemory) return 0;
    uaecptr mem = get_long(ri_ptr + PSSO_RenderInfo_Memory);
    uae_s16 bpr = (uae_s16)get_word(ri_ptr + PSSO_RenderInfo_BytesPerRow);
    uae_u32 fmt = _68k_dreg(7);
    uae_u16 x = (uae_u16)_68k_dreg(0);
    uae_u16 y = (uae_u16)_68k_dreg(1);
    uae_u16 w = (uae_u16)_68k_dreg(2);
    uae_u16 h = (uae_u16)_68k_dreg(3);
    uae_u32 color = _68k_dreg(4);
    uae_u32 mask = _68k_dreg(5) & 0xFF;
    if (w == 0 || h == 0) return 1;
    uae_u8 bpp;
    uae_u32 offset;
    if (!validate_render_target(mem, bpr, fmt, x, y, w, h, &bpp, &offset)) return 0;
    if (mask != 0xFF && bpp != 1)
        return 0;

    for (uae_u16 row = 0; row < h; row++) {
        uae_u8 *dst = gfxmemory + offset + (y + row) * bpr + x * bpp;
        for (uae_u16 col = 0; col < w; col++) {
            uae_u8 *pixel = dst + col * bpp;
            if (mask == 0xFF) {
                write_pixel(pixel, bpp, color);
            } else {
                uae_u32 old = read_pixel(pixel, 1);
                write_pixel(pixel, 1, (color & mask) | (old & ~mask));
            }
        }
    }
    return 1;
}

uae_u32 picasso_BlitRect(void)
{
    uaecptr ri_ptr = _68k_areg(1);
    if (!ri_ptr || !gfxmemory) return 0;
    uaecptr mem = get_long(ri_ptr + PSSO_RenderInfo_Memory);
    uae_s16 bpr = get_word(ri_ptr + PSSO_RenderInfo_BytesPerRow);
    uae_u32 fmt = get_long(ri_ptr + PSSO_RenderInfo_RGBFormat);
    uae_u16 srcx = (uae_u16)_68k_dreg(0);
    uae_u16 srcy = (uae_u16)_68k_dreg(1);
    uae_u16 dstx = (uae_u16)_68k_dreg(2);
    uae_u16 dsty = (uae_u16)_68k_dreg(3);
    uae_u16 w = (uae_u16)_68k_dreg(4);
    uae_u16 h = (uae_u16)_68k_dreg(5);
    uae_u32 mask = _68k_dreg(6) & 0xFF;
    if (w == 0 || h == 0) return 1;
    uae_u8 bpp;
    uae_u32 offset;
    if (!validate_render_target(mem, bpr, fmt, srcx, srcy, w, h, &bpp, &offset)
        || !validate_render_target(mem, bpr, fmt, dstx, dsty, w, h, &bpp, NULL)) return 0;

    size_t line_bytes = (size_t)w * bpp;
    if (bpp == 1 && mask != 0xFF) {
        int reverse = dsty > srcy || (dsty == srcy && dstx > srcx);
        for (int row = reverse ? (int)h - 1 : 0;
             reverse ? row >= 0 : row < h;
             reverse ? --row : ++row) {
            uae_u8 *s = gfxmemory + offset + (srcy + row) * bpr + srcx;
            uae_u8 *d = gfxmemory + offset + (dsty + row) * bpr + dstx;
            if (reverse && d > s) {
                for (int col = (int)w - 1; col >= 0; col--)
                    d[col] = (s[col] & mask) | (d[col] & (uae_u8)~mask);
            } else {
                for (uae_u16 col = 0; col < w; col++)
                    d[col] = (s[col] & mask) | (d[col] & (uae_u8)~mask);
            }
        }
        return 1;
    }
    if (dsty <= srcy) {
        for (uae_u16 row = 0; row < h; row++) {
            uae_u8 *s = gfxmemory + offset + (srcy + row) * bpr + srcx * bpp;
            uae_u8 *d = gfxmemory + offset + (dsty + row) * bpr + dstx * bpp;
            memmove(d, s, line_bytes);
        }
    } else {
        for (int row = (int)h - 1; row >= 0; row--) {
            uae_u8 *s = gfxmemory + offset + (srcy + row) * bpr + srcx * bpp;
            uae_u8 *d = gfxmemory + offset + (dsty + row) * bpr + dstx * bpp;
            memmove(d, s, line_bytes);
        }
    }
    return 1;
}

uae_u32 picasso_BlitRectNoMaskComplete(void)
{
    uaecptr src_ri = _68k_areg(1);
    uaecptr dst_ri = _68k_areg(2);
    uae_u16 srcx = (uae_u16)_68k_dreg(0);
    uae_u16 srcy = (uae_u16)_68k_dreg(1);
    uae_u16 dstx = (uae_u16)_68k_dreg(2);
    uae_u16 dsty = (uae_u16)_68k_dreg(3);
    uae_u16 w = (uae_u16)_68k_dreg(4);
    uae_u16 h = (uae_u16)_68k_dreg(5);
    if (!src_ri || !dst_ri || _68k_dreg(6) != BLIT_SRC || !w || !h) return 0;
    uaecptr src_mem = get_long(src_ri + PSSO_RenderInfo_Memory);
    uaecptr dst_mem = get_long(dst_ri + PSSO_RenderInfo_Memory);
    uae_s16 src_bpr = get_word(src_ri + PSSO_RenderInfo_BytesPerRow);
    uae_s16 dst_bpr = get_word(dst_ri + PSSO_RenderInfo_BytesPerRow);
    uae_u32 src_fmt = get_long(src_ri + PSSO_RenderInfo_RGBFormat);
    uae_u32 dst_fmt = get_long(dst_ri + PSSO_RenderInfo_RGBFormat);
    uae_u8 bpp;
    uae_u32 src_offset, dst_offset;
    if (src_fmt != dst_fmt
        || !validate_render_target(src_mem, src_bpr, src_fmt, srcx, srcy, w, h, &bpp, &src_offset)
        || !validate_render_target(dst_mem, dst_bpr, dst_fmt, dstx, dsty, w, h, &bpp, &dst_offset)) return 0;
    size_t line_bytes = (size_t)w * bpp;
    int reverse = src_mem == dst_mem && dst_offset + dsty * dst_bpr > src_offset + srcy * src_bpr;
    for (int row = reverse ? (int)h - 1 : 0; reverse ? row >= 0 : row < h; reverse ? --row : ++row) {
        uae_u8 *s = gfxmemory + src_offset + (srcy + row) * src_bpr + srcx * bpp;
        uae_u8 *d = gfxmemory + dst_offset + (dsty + row) * dst_bpr + dstx * bpp;
        memmove(d, s, line_bytes);
    }
    return 1;
}

uae_u32 picasso_InvertRect(void)
{
    uaecptr ri_ptr = _68k_areg(1);
    if (!ri_ptr || !gfxmemory) return 0;
    uaecptr mem = get_long(ri_ptr + PSSO_RenderInfo_Memory);
    uae_s16 bpr = get_word(ri_ptr + PSSO_RenderInfo_BytesPerRow);
    uae_u32 fmt = get_long(ri_ptr + PSSO_RenderInfo_RGBFormat);
    uae_u16 x = (uae_u16)_68k_dreg(0);
    uae_u16 y = (uae_u16)_68k_dreg(1);
    uae_u16 w = (uae_u16)_68k_dreg(2);
    uae_u16 h = (uae_u16)_68k_dreg(3);
    if (w == 0 || h == 0) return 1;
    uae_u8 bpp;
    uae_u32 offset;
    if (!validate_render_target(mem, bpr, fmt, x, y, w, h, &bpp, &offset)) return 0;

    size_t line_bytes = (size_t)w * bpp;
    for (uae_u16 row = 0; row < h; row++) {
        uae_u8 *d = gfxmemory + offset + (y + row) * bpr + x * bpp;
        uae_u32 xor_mask = (bpp == 1) ? (uae_u32)(_68k_dreg(4) & 0xFF) : pixel_mask_for_bpp(bpp);
        for (size_t i = 0; i < line_bytes; i++) {
            uae_u8 byte_mask = (uae_u8)(xor_mask >> ((i % bpp) * 8));
            d[i] ^= byte_mask;
        }
    }
    return 1;
}

uae_u32 picasso_BlitTemplate(void)
{
    uaecptr ri_ptr = _68k_areg(1);
    uaecptr tmpl_ptr = _68k_areg(2);
    if (!ri_ptr || !tmpl_ptr || !gfxmemory) return 0;
    uaecptr mem = get_long(ri_ptr + PSSO_RenderInfo_Memory);
    uae_s16 bpr = (uae_s16)get_word(ri_ptr + PSSO_RenderInfo_BytesPerRow);
    uae_u32 fmt = get_long(ri_ptr + PSSO_RenderInfo_RGBFormat);
    uae_u16 x = (uae_u16)_68k_dreg(0);
    uae_u16 y = (uae_u16)_68k_dreg(1);
    uae_u16 w = (uae_u16)_68k_dreg(2);
    uae_u16 h = (uae_u16)_68k_dreg(3);
    uae_u32 mask = _68k_dreg(4) & 0xFF;
    uaecptr tmpl_mem = get_long(tmpl_ptr + PSSO_Template_Memory);
    uae_s16 tmpl_bpr = (uae_s16)get_word(tmpl_ptr + PSSO_Template_BytesPerRow);
    uae_u8 tmpl_xoff = get_byte(tmpl_ptr + PSSO_Template_XOffset);
    uae_u8 draw_mode = get_byte(tmpl_ptr + PSSO_Template_DrawMode);
    uae_u32 fg_pen = get_long(tmpl_ptr + PSSO_Template_FgPen);
    uae_u32 bg_pen = get_long(tmpl_ptr + PSSO_Template_BgPen);
    if (w == 0 || h == 0 || tmpl_bpr <= 0) return 1;
    uae_u8 bpp;
    uae_u32 offset;
    if (!validate_render_target(mem, bpr, fmt, x, y, w, h, &bpp, &offset)) return 0;
    if (mask != 0xFF && bpp != 1) return 0;
    if (!valid_address(tmpl_mem, (size_t)h * tmpl_bpr + ((tmpl_xoff + w + 7) >> 3))) return 0;

    for (uae_u16 row = 0; row < h; row++) {
        uae_u8 *dst = gfxmemory + offset + (y + row) * bpr + x * bpp;
        for (uae_u16 col = 0; col < w; col++) {
            uae_u16 bit_idx = tmpl_xoff + col;
            uae_u8 src_byte = get_byte(tmpl_mem + row * tmpl_bpr + (bit_idx >> 3));
            int bit = (src_byte >> (7 - (bit_idx & 7))) & 1;
            write_draw_pixel(dst + col * bpp, bpp, fg_pen, bg_pen, mask,
                             draw_mode, bit);
        }
    }
    return 1;
}

uae_u32 picasso_BlitPattern(void)
{
    uaecptr ri_ptr = _68k_areg(1);
    uaecptr pat_ptr = _68k_areg(2);
    if (!ri_ptr || !pat_ptr || !gfxmemory) return 0;
    uaecptr mem = get_long(ri_ptr + PSSO_RenderInfo_Memory);
    uae_s16 bpr = (uae_s16)get_word(ri_ptr + PSSO_RenderInfo_BytesPerRow);
    uae_u32 fmt = get_long(ri_ptr + PSSO_RenderInfo_RGBFormat);
    uae_u16 x = (uae_u16)_68k_dreg(0);
    uae_u16 y = (uae_u16)_68k_dreg(1);
    uae_u16 w = (uae_u16)_68k_dreg(2);
    uae_u16 h = (uae_u16)_68k_dreg(3);
    uae_u32 mask = _68k_dreg(4) & 0xFF;
    uaecptr pat_mem = get_long(pat_ptr + PSSO_Pattern_Memory);
    uae_u16 pat_xoff = get_word(pat_ptr + PSSO_Pattern_XOffset);
    uae_u16 pat_yoff = get_word(pat_ptr + PSSO_Pattern_YOffset);
    uae_u32 fg_pen = get_long(pat_ptr + PSSO_Pattern_FgPen);
    uae_u32 bg_pen = get_long(pat_ptr + PSSO_Pattern_BgPen);
    uae_u8 pat_size = get_byte(pat_ptr + PSSO_Pattern_Size);
    uae_u8 draw_mode = get_byte(pat_ptr + PSSO_Pattern_DrawMode);
    if (w == 0 || h == 0) return 1;
    uae_u8 bpp;
    uae_u32 offset;
    if (!validate_render_target(mem, bpr, fmt, x, y, w, h, &bpp, &offset)) return 0;
    if (mask != 0xFF && bpp != 1) return 0;
    if (pat_size > 4) pat_size = 4;
    uae_u16 pat_mask_y = (1 << pat_size) - 1;
    if (!valid_address(pat_mem, ((size_t)1 << pat_size) * 2)) return 0;

    for (uae_u16 row = 0; row < h; row++) {
        uae_u8 *dst = gfxmemory + offset + (y + row) * bpr + x * bpp;
        uae_u16 py = (pat_yoff + row) & pat_mask_y;
        uae_u16 pat_word = get_word(pat_mem + py * 2);
        for (uae_u16 col = 0; col < w; col++) {
            uae_u16 px = (pat_xoff + col) & 15;
            int bit = (pat_word >> (15 - px)) & 1;
            write_draw_pixel(dst + col * bpp, bpp, fg_pen, bg_pen, mask,
                             draw_mode, bit);
        }
    }
    return 1;
}

static uae_u32 p2c_minterm_apply(uae_u32 minterm, uae_u32 src, uae_u32 dst,
                             uae_u32 pixel_mask)
{
    return p96_minterm_apply(minterm, src, dst, pixel_mask);
}

static uae_u32 p96_BlitPlanar2ChunkyCommon(int direct)
{
    uaecptr bm_ptr = _68k_areg(1);
    uaecptr ri_ptr = _68k_areg(2);
    uaecptr cim_ptr = _68k_areg(3);
    uae_u16 srcx = (uae_u16)_68k_dreg(0);
    uae_u16 srcy = (uae_u16)_68k_dreg(1);
    uae_u16 dstx = (uae_u16)_68k_dreg(2);
    uae_u16 dsty = (uae_u16)_68k_dreg(3);
    uae_u16 w = (uae_u16)_68k_dreg(4);
    uae_u16 h = (uae_u16)_68k_dreg(5);
    uae_u32 minterm = _68k_dreg(6) & 0xFF;
    uae_u32 mask = _68k_dreg(7) & 0xFF;

    if (!bm_ptr || !gfxmemory || !w || !h) return 0;

    uaecptr dst_mem;
    uae_s16 dst_bpr;
    uae_u32 dst_fmt;
    if (ri_ptr) {
        dst_mem = get_long(ri_ptr + PSSO_RenderInfo_Memory);
        dst_bpr = (uae_s16)get_word(ri_ptr + PSSO_RenderInfo_BytesPerRow);
        dst_fmt = get_long(ri_ptr + PSSO_RenderInfo_RGBFormat);
    } else {
        dst_mem = picasso96_state.Address;
        dst_bpr = (uae_s16)picasso96_state.BytesPerRow;
        dst_fmt = picasso96_state.RGBFormat;
    }

    if (!direct && dst_fmt != RGBFB_CLUT) return 0;

    uae_u8 bpp;
    uae_u32 dst_offset;
    if (!validate_render_target(dst_mem, dst_bpr, dst_fmt, dstx, dsty, w, h,
                                &bpp, &dst_offset)) return 0;

    uae_u16 bm_bpr = get_word(bm_ptr + PSSO_BitMap_BytesPerRow);
    uae_u16 bm_rows = get_word(bm_ptr + PSSO_BitMap_Rows);
    uae_u8 bm_depth = get_byte(bm_ptr + PSSO_BitMap_Depth);
    if (!bm_bpr || !bm_depth || bm_depth > 8) return 0;
    if ((uae_u32)srcy + h > bm_rows) return 0;
    if (direct && bpp != 2 && bpp != 3 && bpp != 4) return 0;
    if ((uae_u32)srcx + w > (uae_u32)bm_bpr * 8) return 0;

    uae_u32 plane_bytes = (uae_u32)bm_bpr * bm_rows;
    for (uae_u8 p = 0; p < bm_depth; p++) {
        uaecptr plane = get_long(bm_ptr + PSSO_BitMap_Planes + p * 4);
        if (plane != 0 && plane != 0xFFFFFFFFu
            && (!valid_address(plane, plane_bytes))) return 0;
    }

    uae_u32 cim_mask = 0;
    if (direct) {
        if (!cim_ptr) return 0;
        cim_mask = get_long(cim_ptr + PSSO_ColorIndexMapping_ColorMask);
    }

    for (uae_u16 row = 0; row < h; row++) {
        uae_u8 *dst = gfxmemory + dst_offset + (dsty + row) * dst_bpr + dstx * bpp;
        uae_u16 bm_row = srcy + row;
        for (uae_u16 col = 0; col < w; col++) {
            uae_u32 chunky = 0;
            uae_u16 bit_idx = srcx + col;
            for (uae_u8 p = 0; p < bm_depth; p++) {
                uaecptr plane = get_long(bm_ptr + PSSO_BitMap_Planes + p * 4);
                int bit_set = 0;
                if (plane == 0xFFFFFFFFu) {
                    bit_set = 1;
                } else if (plane != 0) {
                    uae_u8 src_byte = get_byte(plane + (uae_u32)bm_row * bm_bpr + (bit_idx >> 3));
                    bit_set = (src_byte >> (7 - (bit_idx & 7))) & 1;
                }
                if (!(mask & (1u << p)))
                    bit_set = 0;
                if (bit_set) chunky |= 1u << p;
            }
            uae_u32 out;
            if (direct) {
                uae_u32 mapped = get_long(cim_ptr + PSSO_ColorIndexMapping_Colors
                                          + (chunky & 0xFF) * 4);
                uae_u32 dst_raw = read_pixel(dst + col * bpp, bpp);
                out = p2c_minterm_apply(minterm, mapped, dst_raw,
                                        pixel_mask_for_bpp(bpp));
                if (out == 0 && (minterm & 0x0F) != BLIT_FALSE
                    && (minterm & 0x0F) != BLIT_NOR)
                    ;
                write_pixel(dst + col * bpp, bpp, out);
            } else {
                uae_u32 dst_pen = dst[col];
                out = p2c_minterm_apply(minterm, chunky, dst_pen, 0xFF);
                dst[col] = (uae_u8)out;
            }
        }
    }
    return 1;
}

uae_u32 picasso_BlitPlanar2Chunky(void)
{
    return p96_BlitPlanar2ChunkyCommon(0);
}

uae_u32 picasso_BlitPlanar2Direct(void)
{
    return p96_BlitPlanar2ChunkyCommon(1);
}

uae_u32 picasso_WaitVerticalSync(void)
{
    return 1;
}

uae_u32 picasso_UnsupportedVector(void)
{
    return 0;
}

uae_u32 picasso_demux(void)
{
    uaecptr sp = _68k_areg(7);
    uae_u32 num = get_long(sp + 4);
    switch (num) {
        case 16: return picasso_FindCard();
        case 17: return picasso_FillRect();
        case 18: return picasso_SetSwitch();
        case 19: return picasso_SetColorArray();
        case 20: return picasso_SetDAC();
        case 21: return picasso_SetGC();
        case 22: return picasso_SetPanning();
        case 23: return picasso_CalculateBytesPerRow();
        case 24: return picasso_BlitPlanar2Chunky();
        case 25: return picasso_BlitRect();
        case 26: return picasso_SetDisplay();
        case 27: return picasso_BlitTemplate();
        case 28: return picasso_BlitRectNoMaskComplete();
        case 29: return picasso_InitCard();
        case 30: return picasso_BlitPattern();
        case 31: return picasso_InvertRect();
        case 32: return picasso_BlitPlanar2Direct();
        case 34: return 0;
        case 35: return allocated_gfxmem ? 1 : 0;
        default: return 0;
    }
}

static uae_u32 picasso_Unsupported(void)
{
    return 0;
}

static uaecptr install_p96_trap(TrapFunction function, const char *name)
{
    uaecptr address = here();
    calltrap(deftrap2(function, 0, name));
    dw(RTS);
    return address;
}

void uaegfx_install_code(uaecptr start)
{
    InitPicasso96();

    org(start);
    p96_CalculateBytesPerRow_addr = install_p96_trap(picasso_CalculateBytesPerRow, "p96_CalculateBytesPerRow");
    p96_SetSwitch_addr = install_p96_trap(picasso_SetSwitch, "p96_SetSwitch");
    p96_SetColorArray_addr = install_p96_trap(picasso_SetColorArray, "p96_SetColorArray");
    p96_SetDAC_addr = install_p96_trap(picasso_SetDAC, "p96_SetDAC");
    p96_SetGC_addr = install_p96_trap(picasso_SetGC, "p96_SetGC");
    p96_SetPanning_addr = install_p96_trap(picasso_SetPanning, "p96_SetPanning");
    p96_SetDisplay_addr = install_p96_trap(picasso_SetDisplay, "p96_SetDisplay");
    p96_AllocCardMem_addr = install_p96_trap(p96_AllocCardMem, "p96_AllocCardMem");
    p96_FreeCardMem_addr = install_p96_trap(p96_FreeCardMem, "p96_FreeCardMem");
    p96_FillRect_addr = install_p96_trap(picasso_FillRect, "p96_FillRect");
    p96_BlitRect_addr = install_p96_trap(picasso_BlitRect, "p96_BlitRect");
    p96_BlitRectNoMaskComplete_addr = install_p96_trap(picasso_BlitRectNoMaskComplete, "p96_BlitRectNoMaskComplete");
    p96_InvertRect_addr = install_p96_trap(picasso_InvertRect, "p96_InvertRect");
    p96_BlitTemplate_addr = install_p96_trap(picasso_BlitTemplate, "p96_BlitTemplate");
    p96_BlitPattern_addr = install_p96_trap(picasso_BlitPattern, "p96_BlitPattern");
    p96_BlitPlanar2Direct_addr = install_p96_trap(picasso_BlitPlanar2Direct, "p96_BlitPlanar2Direct");
    p96_BlitPlanar2Chunky_addr = install_p96_trap(picasso_BlitPlanar2Chunky, "p96_BlitPlanar2Chunky");
    p96_CalculateMemory_addr = install_p96_trap(p96_CalculateMemory, "p96_CalculateMemory");
    p96_GetCompatibleFormats_addr = install_p96_trap(p96_GetCompatibleFormats, "p96_GetCompatibleFormats");
    p96_ResolvePixelClock_addr = install_p96_trap(picasso_Unsupported, "p96_ResolvePixelClock");
    p96_GetPixelClock_addr = install_p96_trap(picasso_Unsupported, "p96_GetPixelClock");
    p96_WaitVerticalSync_addr = install_p96_trap(picasso_WaitVerticalSync, "p96_WaitVerticalSync");
    p96_UnsupportedVector_addr = install_p96_trap(picasso_UnsupportedVector, "p96_UnsupportedVector");
}

void picasso_refresh(void)
{
    if (!picasso_on || !picasso96_state.Address || !gfxmemory || !prSDLScreen) return;

    uae_u16 w = picasso96_state.Width;
    uae_u16 h = picasso96_state.Height;
    uae_u16 bpr = picasso96_state.BytesPerRow;
    uae_u8 source_bpp = get_bytes_per_pixel(picasso96_state.RGBFormat);
    if (w == 0 || h == 0 || bpr == 0 || !source_bpp)
        return;

    uae_u32 offset;
    if (!gfxmem_range(picasso96_state.Address, 1, &offset))
        return;

    uae_u64 visible_offset = (uae_u64)(uae_u32)picasso96_state.YOffset * bpr
                           + (uae_u64)(uae_u32)picasso96_state.XOffset * source_bpp;
    uae_u64 visible_end = visible_offset
                        + (uae_u64)(h - 1) * bpr
                        + (uae_u64)w * source_bpp;
    if (visible_end > allocated_gfxmem - offset) return;
    offset += (uae_u32)visible_offset;

    uae_u8 *src = gfxmemory + offset;

    if (w > (uae_u16)prSDLScreen->w) w = (uae_u16)prSDLScreen->w;
    if (h > (uae_u16)prSDLScreen->h) h = (uae_u16)prSDLScreen->h;

    uae_u8 *dst_base = (uae_u8 *)prSDLScreen->pixels;
    int dst_pitch = prSDLScreen->pitch;
    uae_u32 fmt = picasso96_state.RGBFormat;

    if (fmt == RGBFB_CLUT) {
        for (uae_u16 y = 0; y < h; y++) {
            uae_u8 *s_row = src + y * bpr;
            uae_u16 *d_row = (uae_u16 *)(dst_base + y * dst_pitch);
            for (uae_u16 x = 0; x < w; x++) {
                uae_u8 index = do_get_mem_byte(s_row + x);
                struct MyCLUTEntry c = picasso96_state.CLUT[index];
                d_row[x] = ((c.Red >> 3) << 11) | ((c.Green >> 2) << 5) | (c.Blue >> 3);
            }
        }
    } else if (fmt == RGBFB_R5G6B5PC || fmt == RGBFB_R5G6B5) {
        for (uae_u16 y = 0; y < h; y++) {
            uae_u16 *s_row = (uae_u16 *)(src + y * bpr);
            uae_u16 *d_row = (uae_u16 *)(dst_base + y * dst_pitch);
            if (fmt == RGBFB_R5G6B5PC) {
                memcpy(d_row, s_row, w * 2);
            } else {
                for (uae_u16 x = 0; x < w; x++) {
                    uae_u16 px = s_row[x];
                    d_row[x] = (px >> 8) | (px << 8);
                }
            }
        }
    } else if (fmt == RGBFB_R5G5B5PC || fmt == RGBFB_R5G5B5) {
        for (uae_u16 y = 0; y < h; y++) {
            uae_u16 *s_row = (uae_u16 *)(src + y * bpr);
            uae_u16 *d_row = (uae_u16 *)(dst_base + y * dst_pitch);
            for (uae_u16 x = 0; x < w; x++) {
                uae_u16 px = s_row[x];
                if (fmt == RGBFB_R5G5B5) px = (px >> 8) | (px << 8);
                d_row[x] = ((px & 0x7FE0) << 1) | (px & 0x001F);
            }
        }
    } else if (fmt == RGBFB_B5G6R5PC) {
        for (uae_u16 y = 0; y < h; y++) {
            uae_u16 *s_row = (uae_u16 *)(src + y * bpr);
            uae_u16 *d_row = (uae_u16 *)(dst_base + y * dst_pitch);
            for (uae_u16 x = 0; x < w; x++) {
                uae_u16 px = s_row[x];
                d_row[x] = ((px & 0x001F) << 11) | (px & 0x07E0) | ((px & 0xF800) >> 11);
            }
        }
    } else if (fmt == RGBFB_B5G5R5PC) {
        for (uae_u16 y = 0; y < h; y++) {
            uae_u16 *s_row = (uae_u16 *)(src + y * bpr);
            uae_u16 *d_row = (uae_u16 *)(dst_base + y * dst_pitch);
            for (uae_u16 x = 0; x < w; x++) {
                uae_u16 px = s_row[x];
                d_row[x] = ((px & 0x001F) << 11) | ((px & 0x03E0) << 1) | ((px & 0x7C00) >> 10);
            }
        }
    } else if (fmt == RGBFB_A8R8G8B8 || fmt == RGBFB_R8G8B8A8) {
        for (uae_u16 y = 0; y < h; y++) {
            uae_u8 *s_row = src + y * bpr;
            uae_u16 *d_row = (uae_u16 *)(dst_base + y * dst_pitch);
            for (uae_u16 x = 0; x < w; x++) {
                uae_u8 r = do_get_mem_byte(s_row + x * 4 + (fmt == RGBFB_A8R8G8B8 ? 1 : 0));
                uae_u8 g = do_get_mem_byte(s_row + x * 4 + (fmt == RGBFB_A8R8G8B8 ? 2 : 1));
                uae_u8 b = do_get_mem_byte(s_row + x * 4 + (fmt == RGBFB_A8R8G8B8 ? 3 : 2));
                d_row[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            }
        }
    } else if (fmt == RGBFB_A8B8G8R8 || fmt == RGBFB_B8G8R8A8) {
        for (uae_u16 y = 0; y < h; y++) {
            uae_u8 *s_row = src + y * bpr;
            uae_u16 *d_row = (uae_u16 *)(dst_base + y * dst_pitch);
            for (uae_u16 x = 0; x < w; x++) {
                uae_u8 b = do_get_mem_byte(s_row + x * 4 + (fmt == RGBFB_A8B8G8R8 ? 1 : 0));
                uae_u8 g = do_get_mem_byte(s_row + x * 4 + (fmt == RGBFB_A8B8G8R8 ? 2 : 1));
                uae_u8 r = do_get_mem_byte(s_row + x * 4 + (fmt == RGBFB_A8B8G8R8 ? 3 : 2));
                d_row[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            }
        }
    } else if (fmt == RGBFB_R8G8B8 || fmt == RGBFB_B8G8R8) {
        for (uae_u16 y = 0; y < h; y++) {
            uae_u8 *s_row = src + y * bpr;
            uae_u16 *d_row = (uae_u16 *)(dst_base + y * dst_pitch);
            for (uae_u16 x = 0; x < w; x++) {
                uae_u8 r = do_get_mem_byte(s_row + x * 3 + (fmt == RGBFB_R8G8B8 ? 0 : 2));
                uae_u8 g = do_get_mem_byte(s_row + x * 3 + 1);
                uae_u8 b = do_get_mem_byte(s_row + x * 3 + (fmt == RGBFB_R8G8B8 ? 2 : 0));
                d_row[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            }
        }
    }
}
