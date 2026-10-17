#ifndef PS3_COMPAT_H
#define PS3_COMPAT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

typedef uint8_t u8;
typedef int32_t s32;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int32_t Result;

#define R_SUCCEEDED(res) ((res) >= 0)

typedef enum {
	AppletOperationMode_Handheld = 0,
	AppletOperationMode_Console = 1
} AppletOperationMode;

AppletOperationMode appletGetOperationMode(void);

typedef struct {
	uint16_t x;
	uint16_t y;
} HidTouchScreen;

typedef struct {
	uint32_t count;
	HidTouchScreen touches[10];
} HidTouchScreenState;

u32 hidGetTouchScreenStates(HidTouchScreenState *state, u32 count);

typedef int32_t HidNpadIdType;

typedef enum {
	HidNpadJoyHoldType_Horizontal = 0,
	HidNpadJoyHoldType_Vertical = 1
} HidNpadJoyHoldType;

typedef enum {
	HidNpadStyleTag_NpadLeft = 0,
	HidNpadStyleTag_NpadJoyLeft = 0x00000001,
	HidNpadStyleTag_NpadJoyRight = 0x00000002,
	HidNpadStyleTag_NpadJoyStick = 0x00000003,
	HidNpadStyleTag_FullJoy = 0x00000004
} HidNpadStyleTag;

void hidSetNpadJoyHoldType(HidNpadJoyHoldType type);
void hidSetNpadJoyAssignmentModeSingleByDefault(HidNpadIdType id);
void hidSetNpadJoyAssignmentModeDual(HidNpadIdType id);
HidNpadStyleTag hidGetNpadStyleSet(HidNpadIdType id);
void hidMergeSingleJoyAsDualJoy(HidNpadIdType idSingle, HidNpadIdType idDual);

typedef struct {
	u64 buttons;
} PadState;

typedef struct {
	s32 x;
	s32 y;
} HidAnalogStickState;

void padInitializeDefault(PadState *pad);
void padUpdate(PadState *pad);
u64 padGetButtons(const PadState *pad);
HidAnalogStickState padGetStickPos(const PadState *pad, u32 stick);

typedef enum {
	HidNpadButton_Up = 0x0000000000000001ULL,
	HidNpadButton_Down = 0x0000000000000002ULL,
	HidNpadButton_Left = 0x0000000000000004ULL,
	HidNpadButton_Right = 0x0000000000000008ULL,
	HidNpadButton_X = 0x0000000000000010ULL,
	HidNpadButton_Y = 0x0000000000000020ULL,
	HidNpadButton_A = 0x0000000000000040ULL,
	HidNpadButton_B = 0x0000000000000080ULL,
	HidNpadButton_L = 0x0000000000000100ULL,
	HidNpadButton_R = 0x0000000000000200ULL,
	HidNpadButton_ZL = 0x0000000000000400ULL,
	HidNpadButton_ZR = 0x0000000000000800ULL,
	HidNpadButton_Minus = 0x0000000000001000ULL,
	HidNpadButton_Plus = 0x0000000000002000ULL,
	HidNpadButton_LStick = 0x0000000000040000ULL,
	HidNpadButton_RStick = 0x0000000000080000ULL
} HidNpadButton;

typedef struct {
	u32 reserved;
	char text[1024];
} SwkbdConfig;

Result swkbdCreate(SwkbdConfig *kbd, u32 unk);
void swkbdConfigMakePresetDefault(SwkbdConfig *kbd);
void swkbdConfigSetGuideText(SwkbdConfig *kbd, const char *guide);
void swkbdConfigSetHeaderText(SwkbdConfig *kbd, const char *header);
void swkbdConfigSetInitialText(SwkbdConfig *kbd, const char *text);
void swkbdConfigSetStringLenMax(SwkbdConfig *kbd, size_t max);
Result swkbdShow(SwkbdConfig *kbd, char *buf, size_t len);
void swkbdClose(SwkbdConfig *kbd);

void romfsInit(void);
void romfsExit(void);

u64 svcGetSystemTick(void);

void update_pad_mode(void);
void ps3_exit(int quit_via_home);

typedef enum {
	PsmChargerType_Unconnected = 0,
	PsmChargerType_AC = 1,
	PsmChargerType_USB = 2
} PsmChargerType;

typedef enum {
	NifmServiceType_Start = 1,
	NifmServiceType_User = 2
} NifmServiceType;

typedef struct { unsigned char data[16]; } Uuid;

typedef struct { int reserved; } WebWifiConfig;

Result webWifiCreate(WebWifiConfig *config, void *scratch, const char *url, Uuid uuid, u32 option);
void webWifiShow(WebWifiConfig *config, void *scratch);
void webWifiDestroy(WebWifiConfig *config);

Result socketInitializeDefault(void);
void socketExit(void);

Result psmInitialize(void);
void psmExit(void);
Result psmGetBatteryChargePercentage(u32 *pct);
Result psmGetChargerType(PsmChargerType *type);

Result nifmInitialize(NifmServiceType type);
Result nifmGetCurrentIpAddress(u32 address[4]);
void nifmExit(void);

#ifdef __cplusplus
}
#endif

#endif
