#ifndef UAE_PICASSO96_H
#define UAE_PICASSO96_H

#include "sysdeps.h"

#define JAM1 0
#define JAM2 1
#define COMP 2
#define INVERS 4

typedef enum {
    BLIT_FALSE,
    BLIT_NOR,
    BLIT_ONLYDST,
    BLIT_NOTSRC,
    BLIT_ONLYSRC,
    BLIT_NOTDST,
    BLIT_EOR,
    BLIT_NAND,
    BLIT_AND,
    BLIT_NEOR,
    BLIT_DST,
    BLIT_NOTONLYSRC,
    BLIT_SRC,
    BLIT_NOTONLYDST,
    BLIT_OR,
    BLIT_TRUE,
    BLIT_LAST
} BLIT_OPCODE;

typedef enum {
    BT_NoBoard,
    BT_oMniBus,
    BT_Graffity,
    BT_CyberVision,
    BT_Domino,
    BT_Merlin,
    BT_PicassoII,
    BT_Piccolo,
    BT_RetinaBLT,
    BT_Spectrum,
    BT_PicassoIV,
    BT_PiccoloSD64,
    BT_A2410,
    BT_Pixel64,
    BT_uaegfx,
    BT_CVision3D,
    BT_Altais,
    BT_Prototype1,
    BT_Prototype2,
    BT_Prototype3,
    BT_Prototype4,
    BT_Prototype5,
    BT_MaxBoardTypes
} BTYPE;

struct ScreenResolution {
    uae_u32 width;
    uae_u32 height;
};

#define MAX_PICASSO_MODES 64

struct PicassoResolution {
    struct ScreenResolution res;
    int depth;
    int refresh;
    char name[25];
};

extern struct PicassoResolution DisplayModes[MAX_PICASSO_MODES];

typedef enum {
    RGBFB_NONE,
    RGBFB_CLUT,
    RGBFB_R8G8B8,
    RGBFB_B8G8R8,
    RGBFB_R5G6B5PC,
    RGBFB_R5G5B5PC,
    RGBFB_A8R8G8B8,
    RGBFB_A8B8G8R8,
    RGBFB_R8G8B8A8,
    RGBFB_B8G8R8A8,
    RGBFB_R5G6B5,
    RGBFB_R5G5B5,
    RGBFB_B5G6R5PC,
    RGBFB_B5G5R5PC,
    RGBFB_Y4U2V2,
    RGBFB_Y4U1V1,
    RGBFB_MaxFormats
} RGBFTYPE;

#define RGBFF_NONE (1<<RGBFB_NONE)
#define RGBFF_CLUT (1<<RGBFB_CLUT)
#define RGBFF_R8G8B8 (1<<RGBFB_R8G8B8)
#define RGBFF_B8G8R8 (1<<RGBFB_B8G8R8)
#define RGBFF_R5G6B5PC (1<<RGBFB_R5G6B5PC)
#define RGBFF_R5G5B5PC (1<<RGBFB_R5G5B5PC)
#define RGBFF_A8R8G8B8 (1<<RGBFB_A8R8G8B8)
#define RGBFF_A8B8G8R8 (1<<RGBFB_A8B8G8R8)
#define RGBFF_R8G8B8A8 (1<<RGBFB_R8G8B8A8)
#define RGBFF_B8G8R8A8 (1<<RGBFB_B8G8R8A8)
#define RGBFF_R5G6B5 (1<<RGBFB_R5G6B5)
#define RGBFF_R5G5B5 (1<<RGBFB_R5G5B5)
#define RGBFF_B5G6R5PC (1<<RGBFB_B5G6R5PC)
#define RGBFF_B5G5R5PC (1<<RGBFB_B5G5R5PC)
#define RGBFF_Y4U2V2 (1<<RGBFB_Y4U2V2)
#define RGBFF_Y4U1V1 (1<<RGBFB_Y4U1V1)

#define RGBMASK_8BIT RGBFF_CLUT
#define RGBMASK_16BIT (RGBFF_R5G6B5PC | RGBFF_B5G6R5PC | RGBFF_R5G6B5)
#define RGBMASK_15BIT (RGBFF_R5G5B5PC | RGBFF_B5G5R5PC | RGBFF_R5G5B5)
#define RGBMASK_24BIT (RGBFF_R8G8B8 | RGBFF_B8G8R8)
#define RGBMASK_32BIT (RGBFF_A8R8G8B8 | RGBFF_A8B8G8R8 | RGBFF_R8G8B8A8 | RGBFF_B8G8R8A8)

#define RGBFF_PLANAR RGBFF_NONE
#define RGBFF_CHUNKY RGBFF_CLUT
#define RGBFB_PLANAR RGBFB_NONE
#define RGBFB_CHUNKY RGBFB_CLUT

enum {
    PLANAR,
    CHUNKY,
    HICOLOR,
    TRUECOLOR,
    TRUEALPHA,
    MAXMODES
};

struct MyCLUTEntry {
    uae_u8 Red;
    uae_u8 Green;
    uae_u8 Blue;
    uae_u8 Pad;
};

struct ColorIndexMapping {
    uae_u32 ColorMask;
    uae_u32 Colors[256];
};

struct RenderInfo {
    uae_u8 *Memory;
    uae_s16 BytesPerRow;
    uae_s16 pad;
    RGBFTYPE RGBFormat;
    uaecptr AMemory;
};

struct Pattern {
    char *Memory;
    uae_u16 XOffset, YOffset;
    uae_u32 FgPen, BgPen;
    uae_u8 Size;
    uae_u8 DrawMode;
};

struct Template {
    char *Memory;
    uae_s16 BytesPerRow;
    uae_u8 XOffset;
    uae_u8 DrawMode;
    uae_u32 FgPen;
    uae_u32 BgPen;
};

#define MAXRESOLUTIONNAMELENGTH 22

struct LibResolution {
    char P96ID[6];
    char Name[MAXRESOLUTIONNAMELENGTH];
    uae_u32 DisplayID;
    uae_u16 Width;
    uae_u16 Height;
    uae_u16 Flags;
    uaecptr Modes[MAXMODES];
    uaecptr BoardInfo;
};

#define P96F_PUBLIC 2

#define PSSO_ModeInfo_OpenCount 14
#define PSSO_ModeInfo_Active 16
#define PSSO_ModeInfo_Width 18
#define PSSO_ModeInfo_Height 20
#define PSSO_ModeInfo_Depth 22
#define PSSO_ModeInfo_Flags 23
#define PSSO_ModeInfo_HorTotal 24
#define PSSO_ModeInfo_HorBlankSize 26
#define PSSO_ModeInfo_HorSyncStart 28
#define PSSO_ModeInfo_HorSyncSize 30
#define PSSO_ModeInfo_HorSyncSkew 32
#define PSSO_ModeInfo_HorEnableSkew 33
#define PSSO_ModeInfo_VerTotal 34
#define PSSO_ModeInfo_VerBlankSize 36
#define PSSO_ModeInfo_VerSyncStart 38
#define PSSO_ModeInfo_VerSyncSize 40
#define PSSO_ModeInfo_first_union 42
#define PSSO_ModeInfo_second_union 43
#define PSSO_ModeInfo_PixelClock 44
#define PSSO_ModeInfo_sizeof 48

#define PSSO_RenderInfo_Memory 0
#define PSSO_RenderInfo_BytesPerRow 4
#define PSSO_RenderInfo_pad 6
#define PSSO_RenderInfo_RGBFormat 8
#define PSSO_RenderInfo_sizeof 12

#define PSSO_BitMap_BytesPerRow 0
#define PSSO_BitMap_Rows 2
#define PSSO_BitMap_Flags 4
#define PSSO_BitMap_Depth 5
#define PSSO_BitMap_pad 6
#define PSSO_BitMap_Planes 8
#define PSSO_BitMap_sizeof 40

#define PSSO_ColorIndexMapping_ColorMask 0
#define PSSO_ColorIndexMapping_Colors 4

#define PSSO_Pattern_Memory 0
#define PSSO_Pattern_XOffset 4
#define PSSO_Pattern_YOffset 6
#define PSSO_Pattern_FgPen 8
#define PSSO_Pattern_BgPen 12
#define PSSO_Pattern_Size 16
#define PSSO_Pattern_DrawMode 17
#define PSSO_Pattern_sizeof 18

#define PSSO_Template_Memory 0
#define PSSO_Template_BytesPerRow 4
#define PSSO_Template_XOffset 6
#define PSSO_Template_DrawMode 7
#define PSSO_Template_FgPen 8
#define PSSO_Template_BgPen 12
#define PSSO_Template_sizeof 16

#define PSSO_LibResolution_P96ID 14
#define PSSO_LibResolution_Name 20
#define PSSO_LibResolution_DisplayID 42
#define PSSO_LibResolution_Width 46
#define PSSO_LibResolution_Height 48
#define PSSO_LibResolution_Flags 50
#define PSSO_LibResolution_Modes 52
#define PSSO_LibResolution_BoardInfo (52 + MAXMODES*4)
#define PSSO_LibResolution_sizeof (60 + MAXMODES*4)

#define PSSO_BoardInfo_RegisterBase 0
#define PSSO_BoardInfo_MemoryBase (PSSO_BoardInfo_RegisterBase + 4)
#define PSSO_BoardInfo_MemoryIOBase (PSSO_BoardInfo_MemoryBase + 4)
#define PSSO_BoardInfo_MemorySize (PSSO_BoardInfo_MemoryIOBase + 4)
#define PSSO_BoardInfo_BoardName (PSSO_BoardInfo_MemorySize + 4)
#define PSSO_BoardInfo_VBIName (PSSO_BoardInfo_BoardName + 4)
#define PSSO_BoardInfo_CardBase (PSSO_BoardInfo_VBIName + 32)
#define PSSO_BoardInfo_ChipBase (PSSO_BoardInfo_CardBase + 4)
#define PSSO_BoardInfo_ExecBase (PSSO_BoardInfo_ChipBase + 4)
#define PSSO_BoardInfo_UtilBase (PSSO_BoardInfo_ExecBase + 4)
#define PSSO_BoardInfo_HardInterrupt (PSSO_BoardInfo_UtilBase + 4)
#define PSSO_BoardInfo_SoftInterrupt (PSSO_BoardInfo_HardInterrupt + 22)
#define PSSO_BoardInfo_BoardLock (PSSO_BoardInfo_SoftInterrupt + 22)
#define PSSO_BoardInfo_ResolutionsList (PSSO_BoardInfo_BoardLock + 46)
#define PSSO_BoardInfo_BoardType (PSSO_BoardInfo_ResolutionsList + 12)
#define PSSO_BoardInfo_PaletteChipType (PSSO_BoardInfo_BoardType + 4)
#define PSSO_BoardInfo_GraphicsControllerType (PSSO_BoardInfo_PaletteChipType + 4)
#define PSSO_BoardInfo_MoniSwitch (PSSO_BoardInfo_GraphicsControllerType + 4)
#define PSSO_BoardInfo_BitsPerCannon (PSSO_BoardInfo_MoniSwitch + 2)
#define PSSO_BoardInfo_Flags (PSSO_BoardInfo_BitsPerCannon + 2)
#define PSSO_BoardInfo_SoftSpriteFlags (PSSO_BoardInfo_Flags + 4)
#define PSSO_BoardInfo_ChipFlags (PSSO_BoardInfo_SoftSpriteFlags + 2)
#define PSSO_BoardInfo_CardFlags (PSSO_BoardInfo_ChipFlags + 2)
#define PSSO_BoardInfo_BoardNum (PSSO_BoardInfo_CardFlags + 4)
#define PSSO_BoardInfo_RGBFormats (PSSO_BoardInfo_BoardNum + 2)
#define PSSO_BoardInfo_MaxHorValue (PSSO_BoardInfo_RGBFormats + 2)
#define PSSO_BoardInfo_MaxVerValue (PSSO_BoardInfo_MaxHorValue + MAXMODES * 2)
#define PSSO_BoardInfo_MaxHorResolution (PSSO_BoardInfo_MaxVerValue + MAXMODES * 2)
#define PSSO_BoardInfo_MaxVerResolution (PSSO_BoardInfo_MaxHorResolution + MAXMODES * 2)
#define PSSO_BoardInfo_MaxMemorySize (PSSO_BoardInfo_MaxVerResolution + MAXMODES * 2)
#define PSSO_BoardInfo_MaxChunkSize (PSSO_BoardInfo_MaxMemorySize + 4)
#define PSSO_BoardInfo_MemoryClock (PSSO_BoardInfo_MaxChunkSize + 4)
#define PSSO_BoardInfo_PixelClockCount (PSSO_BoardInfo_MemoryClock + 4)
#define PSSO_BoardInfo_AllocCardMem (PSSO_BoardInfo_PixelClockCount + MAXMODES * 4)
#define PSSO_BoardInfo_FreeCardMem (PSSO_BoardInfo_AllocCardMem + 4)
#define PSSO_BoardInfo_SetSwitch (PSSO_BoardInfo_FreeCardMem + 4)
#define PSSO_BoardInfo_SetColorArray (PSSO_BoardInfo_SetSwitch + 4)
#define PSSO_BoardInfo_SetDAC (PSSO_BoardInfo_SetColorArray + 4)
#define PSSO_BoardInfo_SetGC (PSSO_BoardInfo_SetDAC + 4)
#define PSSO_BoardInfo_SetPanning (PSSO_BoardInfo_SetGC + 4)
#define PSSO_BoardInfo_CalculateBytesPerRow (PSSO_BoardInfo_SetPanning + 4)
#define PSSO_BoardInfo_CalculateMemory (PSSO_BoardInfo_CalculateBytesPerRow + 4)
#define PSSO_BoardInfo_GetCompatibleFormats (PSSO_BoardInfo_CalculateMemory + 4)
#define PSSO_BoardInfo_SetDisplay (PSSO_BoardInfo_GetCompatibleFormats + 4)
#define PSSO_BoardInfo_ResolvePixelClock (PSSO_BoardInfo_SetDisplay + 4)
#define PSSO_BoardInfo_GetPixelClock (PSSO_BoardInfo_ResolvePixelClock + 4)
#define PSSO_BoardInfo_SetClock (PSSO_BoardInfo_GetPixelClock + 4)
#define PSSO_BoardInfo_SetMemoryMode (PSSO_BoardInfo_SetClock + 4)
#define PSSO_BoardInfo_SetWriteMask (PSSO_BoardInfo_SetMemoryMode + 4)
#define PSSO_BoardInfo_SetClearMask (PSSO_BoardInfo_SetWriteMask + 4)
#define PSSO_BoardInfo_SetReadPlane (PSSO_BoardInfo_SetClearMask + 4)
#define PSSO_BoardInfo_WaitVerticalSync (PSSO_BoardInfo_SetReadPlane + 4)
#define PSSO_BoardInfo_SetInterrupt (PSSO_BoardInfo_WaitVerticalSync + 4)
#define PSSO_BoardInfo_WaitBlitter (PSSO_BoardInfo_SetInterrupt + 4)
#define PSSO_BoardInfo_ScrollPlanar (PSSO_BoardInfo_WaitBlitter + 4)
#define PSSO_BoardInfo_ScrollPlanarDefault (PSSO_BoardInfo_ScrollPlanar + 4)
#define PSSO_BoardInfo_UpdatePlanar (PSSO_BoardInfo_ScrollPlanarDefault + 4)
#define PSSO_BoardInfo_UpdatePlanarDefault (PSSO_BoardInfo_UpdatePlanar + 4)
#define PSSO_BoardInfo_BlitPlanar2Chunky (PSSO_BoardInfo_UpdatePlanarDefault + 4)
#define PSSO_BoardInfo_BlitPlanar2ChunkyDefault (PSSO_BoardInfo_BlitPlanar2Chunky + 4)
#define PSSO_BoardInfo_FillRect (PSSO_BoardInfo_BlitPlanar2ChunkyDefault + 4)
#define PSSO_BoardInfo_FillRectDefault (PSSO_BoardInfo_FillRect + 4)
#define PSSO_BoardInfo_InvertRect (PSSO_BoardInfo_FillRectDefault + 4)
#define PSSO_BoardInfo_InvertRectDefault (PSSO_BoardInfo_InvertRect + 4)
#define PSSO_BoardInfo_BlitRect (PSSO_BoardInfo_InvertRectDefault + 4)
#define PSSO_BoardInfo_BlitRectDefault (PSSO_BoardInfo_BlitRect + 4)
#define PSSO_BoardInfo_BlitTemplate (PSSO_BoardInfo_BlitRectDefault + 4)
#define PSSO_BoardInfo_BlitTemplateDefault (PSSO_BoardInfo_BlitTemplate + 4)
#define PSSO_BoardInfo_BlitPattern (PSSO_BoardInfo_BlitTemplateDefault + 4)
#define PSSO_BoardInfo_BlitPatternDefault (PSSO_BoardInfo_BlitPattern + 4)
#define PSSO_BoardInfo_DrawLine (PSSO_BoardInfo_BlitPatternDefault + 4)
#define PSSO_BoardInfo_DrawLineDefault (PSSO_BoardInfo_DrawLine + 4)
#define PSSO_BoardInfo_BlitRectNoMaskComplete (PSSO_BoardInfo_DrawLineDefault + 4)
#define PSSO_BoardInfo_BlitRectNoMaskCompleteDefault (PSSO_BoardInfo_BlitRectNoMaskComplete + 4)
#define PSSO_BoardInfo_BlitPlanar2Direct (PSSO_BoardInfo_BlitRectNoMaskCompleteDefault + 4)
#define PSSO_BoardInfo_BlitPlanar2DirectDefault (PSSO_BoardInfo_BlitPlanar2Direct + 4)
#define PSSO_BoardInfo_SpecialFeatures (PSSO_BoardInfo_AllocCardMem + 68 * 4)
#define PSSO_BoardInfo_ModeInfo (PSSO_BoardInfo_SpecialFeatures + 12)
#define PSSO_BoardInfo_RGBFormat (PSSO_BoardInfo_ModeInfo + 4)
#define PSSO_BoardInfo_XOffset (PSSO_BoardInfo_RGBFormat + 4)
#define PSSO_BoardInfo_YOffset (PSSO_BoardInfo_XOffset + 2)
#define PSSO_BoardInfo_Depth (PSSO_BoardInfo_YOffset + 2)
#define PSSO_BoardInfo_ClearMask (PSSO_BoardInfo_Depth + 1)
#define PSSO_BoardInfo_Border (PSSO_BoardInfo_ClearMask + 1)
#define PSSO_BoardInfo_Mask (PSSO_BoardInfo_Border + 2)
#define PSSO_BoardInfo_CLUT (PSSO_BoardInfo_Mask + 4)
#define PSSO_BoardInfo_ViewPort (PSSO_BoardInfo_CLUT + 3 * 256)
#define PSSO_BoardInfo_VisibleBitMap (PSSO_BoardInfo_ViewPort + 4)
#define PSSO_BoardInfo_BitMapExtra (PSSO_BoardInfo_VisibleBitMap + 4)
#define PSSO_BitMapExtra_Width 40
#define PSSO_BitMapExtra_Height 42
#define PSSO_BitMapExtra_Flags 44
#define PSSO_BitMapExtra_BaseLevel 46
#define PSSO_BitMapExtra_CurrentLevel 48
#define PSSO_BitMapExtra_CompanionMaster 50
#define PSSO_BitMapExtra_Last 54
#define PSSO_BoardInfo_BitMapList (PSSO_BoardInfo_BitMapExtra + 4)

struct BoardInfo {
    uae_u8 *RegisterBase, *MemoryBase, *MemoryIOBase;
    uae_u32 MemorySize;
    char *BoardName, VBIName[32];
    uae_u16 MoniSwitch;
    uae_u16 BitsPerCannon;
    uae_u32 Flags;
    uae_u16 SoftSpriteFlags;
    uae_u16 ChipFlags;
    uae_u32 CardFlags;
    uae_u16 BoardNum;
    uae_s16 RGBFormats;
    uae_u16 MaxHorValue[MAXMODES];
    uae_u16 MaxVerValue[MAXMODES];
    uae_u16 MaxHorResolution[MAXMODES];
    uae_u16 MaxVerResolution[MAXMODES];
    uae_u32 MaxMemorySize, MaxChunkSize;
};

struct picasso96_state_struct {
    uae_u32 RGBFormat;
    struct MyCLUTEntry CLUT[256];
    uaecptr Address;
    uaecptr Extent;
    uae_u16 Width;
    uae_u16 VirtualWidth;
    uae_u16 BytesPerRow;
    uae_u16 Height;
    uae_u16 VirtualHeight;
    uae_u8 GC_Depth;
    uae_u8 GC_Flags;
    long XOffset;
    long YOffset;
    uae_u8 SwitchState;
    uae_u8 BytesPerPixel;
    uae_u8 CardFound;
};

extern struct picasso96_state_struct picasso96_state;
extern int picasso_on;

void InitPicasso96(void);
void picasso_memory_reset(void);
void picasso_reset(void);
void picasso_shutdown(void);
void picasso_restore_begin(void);
void picasso_restore_pending(void);
void picasso_restore_after_mapping(void);
void picasso_refresh(void);

uae_u8 *picasso_save_state(int *len);
uae_u8 *picasso_save_vram(int *len);
void picasso_restore_state(uae_u8 *data, int len);
void picasso_restore_vram(long filepos, uae_u32 len);

uae_u32 picasso_FindCard(void);
uae_u32 picasso_InitCard(void);
uae_u32 picasso_SetSwitch(void);
uae_u32 picasso_SetDisplay(void);
uae_u32 picasso_SetPanning(void);
uae_u32 picasso_CalculateBytesPerRow(void);
uae_u32 picasso_SetColorArray(void);
uae_u32 picasso_SetDAC(void);
uae_u32 picasso_SetGC(void);
uae_u32 picasso_FillRect(void);
uae_u32 picasso_BlitRect(void);
uae_u32 picasso_InvertRect(void);
uae_u32 picasso_BlitTemplate(void);
uae_u32 picasso_BlitPattern(void);
uae_u32 picasso_BlitPlanar2Direct(void);
uae_u32 picasso_BlitPlanar2Chunky(void);
uae_u32 picasso_UnsupportedVector(void);
uae_u32 picasso_demux(void);

void uaegfx_install_code(uaecptr start);

#endif
