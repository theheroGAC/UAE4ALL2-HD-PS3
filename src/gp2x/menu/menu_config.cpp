#define _MENU_CONFIG_CPP

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "menu.h"
#include "menu_config.h"
#include "autoconf.h"
#include "options.h"
#include "gui.h"
#include "sound.h"
#include "disk_sound.h"
#include "custom.h"
#include "gp2x.h"
#include "cfgfile.h"
#include "cdrom.h"
#if defined(__PSP2__) || defined(__PS3__)
#include "midi_synth.h"
#endif

extern int kickstart;
extern int bReloadKickstart;
extern int blitter_in_partial_mode;
extern int sound_rate;
extern int skipintro;
extern int screenWidth;
extern int moveX;
extern int moveY;


#ifdef __PS3__
#include "ps3_kbd.h"
#include "ps3/whdload_manager_ps3.h"
extern "C" int ps3_set_kickstart(int index, int load_rom);
#endif

#ifdef __PSP2__
#include "psp2_kbdvita.h"
#endif

#if !( defined(PANDORA) || defined(ANDROIDSDL) )
extern int timeslice_mode;
#endif
extern char launchDir[300];
extern char currentDir[300];
extern int nr_drives;
#if defined(__PSP2__) || defined(__PS3__)
static char config_filename[255] = "uae4all.cfg";
#else
extern char *config_filename;
#endif

extern void extractFileName(char * str,char *buffer);


char filename0[256] = "";
char filename1[256] = "";
char filename2[256] = "";
char filename3[256] = "";

char config_load_filename[300] = "";

int mainMenu_chipMemory = DEFAULT_CHIPMEM_SELECT;
int mainMenu_slowMemory = 0;
int mainMenu_fastMemory = 0;
int mainMenu_rtgMemory = 0;

int mainMenu_bootHD = DEFAULT_ENABLE_HD;
int mainMenu_filesysUnits = 0;

int mainMenu_drives = DEFAULT_DRIVES;
int mainMenu_floppyspeed = 100;
int mainMenu_CPU_model = DEFAULT_CPU_MODEL;
int mainMenu_chipset = DEFAULT_CHIPSET_SELECT;
int mainMenu_spriteCollisions = DEFAULT_SPRITECOLLISIONS;
int mainMenu_sound = DEFAULT_SOUND;
int mainMenu_soundStereo = 1;
int mainMenu_soundStereoSep = 4;
int mainMenu_diskSoundVolume = 35;
int mainMenu_CPU_speed = 0;

int mainMenu_cpuSpeed = 600;

int mainMenu_joyConf = 0;
int mainMenu_joyPort = 2;
int mainMenu_autofireRate = 8;
int mainMenu_customAutofireButton = 0;
int mainMenu_showStatus = DEFAULT_STATUSLN;
int mainMenu_background = 0;
int mainMenu_font = 0;
int mainMenu_mouseMultiplier = DEFAULT_MOUSEMULTIPLIER;
int mainMenu_mouseEmulation = 1;
int mainMenu_mouseDevice = 0;
int mainMenu_mouseAcceleration = 0;
int mainMenu_mouseSlowFactor = 2;
int mainMenu_mouseSlowButton = 1;
int mainMenu_mouseFastFactor = 2;
int mainMenu_mouseFastButton = 2;
int mainMenu_mouseSwapButtons = 0;
int mainMenu_stylusOffset = 0;
int mainMenu_tapDelay = 10;
int mainMenu_customControls = 0;
int mainMenu_custom_currentlyEditingControllerNr = 0;
int mainMenu_custom_dpad = 0;
int mainMenu_custom_up[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_down[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_left[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_right[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_stickup[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_stickdown[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_stickleft[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_stickright[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_A[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_B[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_X[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_Y[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_L[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_R[MAX_NUM_CONTROLLERS] = { };
#ifdef __PS3__
int mainMenu_custom_L2[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_R2[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_L3[MAX_NUM_CONTROLLERS] = { };
int mainMenu_custom_R3[MAX_NUM_CONTROLLERS] = { };
#endif
#if defined(__PSP2__) || defined(__PS3__)
int mainMenu_custom_controlSet = 0;
int mainMenu_customPreset_up[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_down[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_left[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_right[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_stickup[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_stickdown[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_stickleft[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_stickright[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_A[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_B[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_X[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_Y[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_L[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_R[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
#ifdef __PS3__
int mainMenu_customPreset_L2[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_R2[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_L3[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
int mainMenu_customPreset_R3[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS] = { };
#endif
#endif
int mainMenu_autofire = 0;
int mainMenu_autofireMode = 0;

int mainMenu_displayedLines = 240;
int mainMenu_displayHires = 0;
char presetMode[20] = "320x240 upscaled";
int presetModeId = 2;
int mainMenu_cutLeft = 0;
int mainMenu_cutRight = 0;
int mainMenu_footerSize = 0;
int mainMenu_screenOffsetY = 0;
int mainMenu_screenOffsetX = 0;
int mainMenu_autoCrop = 0;
int mainMenu_ntsc = DEFAULT_NTSC;
int mainMenu_frameskip = 0;
int mainMenu_vkbdLanguage = 0;
int mainMenu_vkbdStyle = 0;
int mainMenu_vkbdTransparency = 1;
int mainMenu_vkbdPosition = 0;
int visibleAreaWidth = 320;


int saveMenu_n_savestate = 0;

#if defined(__PSP2__) || defined(__PS3__)
#if defined(__PS3__)
int mainMenu_shader = 0;
#else
int mainMenu_shader = 5;
#endif
int mainMenu_leftStickMouse = 0;
int mainMenu_touchControls = 1;
int mainMenu_deadZone = 1000;
int mainMenu_autoEjectFloppy = 1;
int mainMenu_midiSynth = 0;
int mainMenu_pinballMode = 0;
char mainMenu_whdload_game[256] = "";
char mainMenu_whdload_args[256] = "";
int mainMenu_whdload_mode = 0;
int mainMenu_whdload_blitter = 0;
int mainMenu_floppyWriteProtect[4] = { 0, 0, 0, 0 };
int mainMenu_cycleExact = 0;
#endif
#ifdef __PS3__
int mainMenu_swapAB = DEFAULT_SWAPAB;
int mainMenu_singleJoycons = DEFAULT_SINGLEJOYCONS;
int mainMenu_numPlayers = 1;
#endif

int mainMenu_autosave = DEFAULT_AUTOSAVE;
int mainMenu_button1 = GP2X_BUTTON_X;
int mainMenu_button2 = GP2X_BUTTON_A;
int mainMenu_autofireButton1 = GP2X_BUTTON_B;
int mainMenu_jump = -1;

int gp2xClockSpeed = -1;
int mainMenu_scanlines = 0;
int mainMenu_enableScreenshots = DEFAULT_ENABLESCREENSHOTS;
int mainMenu_enableScripts = DEFAULT_ENABLESCRIPTS;
int mainMenu_quickSwitch = 0;

#ifdef ANDROIDSDL
int mainMenu_onScreen = 1;
int mainMenu_onScreen_textinput = 1;
int mainMenu_onScreen_dpad = 1;
int mainMenu_onScreen_button1 = 1;
int mainMenu_onScreen_button2 = 1;
int mainMenu_onScreen_button3 = 1;
int mainMenu_onScreen_button4 = 1;
int mainMenu_onScreen_button5 = 0;
int mainMenu_onScreen_button6 = 0;
int mainMenu_custom_position = 0;
int mainMenu_pos_x_textinput = 0;
int mainMenu_pos_y_textinput = 0;
int mainMenu_pos_x_dpad = 4;
int mainMenu_pos_y_dpad = 215;
int mainMenu_pos_x_button1 = 430;
int mainMenu_pos_y_button1 = 286;
int mainMenu_pos_x_button2 = 378;
int mainMenu_pos_y_button2 = 286;
int mainMenu_pos_x_button3 = 430;
int mainMenu_pos_y_button3 = 214;
int mainMenu_pos_x_button4 = 378;
int mainMenu_pos_y_button4 = 214;
int mainMenu_pos_x_button5 = 430;
int mainMenu_pos_y_button5 = 142;
int mainMenu_pos_x_button6 = 378;
int mainMenu_pos_y_button6 = 142;
float mainMenu_button_size = 1.0;
int menuLoad_extfilter=1;
int mainMenu_FloatingJoystick=0;
#endif
#if defined(ANDROIDSDL) || defined(AROS)
int mainMenu_vsync=0;
#endif
char custom_kickrom[256] = "./kick.rom\0";
int mainMenu_useSavesFolder = DEFAULT_USESAVESFOLDER;

void SetDefaultMenuSettings(int general)
{
    mainMenu_chipMemory = DEFAULT_CHIPMEM_SELECT;
    mainMenu_slowMemory = 0;
    mainMenu_fastMemory = 0;
    UpdateMemorySettings();

	if(general < 2)
	{
		mainMenu_bootHD=DEFAULT_ENABLE_HD;
		kill_hd_configs();
    }

    if(general > 0) {
        uae4all_image_file0[0] = '\0';
        uae4all_image_file1[0] = '\0';
        uae4all_image_file2[0] = '\0';
        uae4all_image_file3[0] = '\0';

        mainMenu_drives = DEFAULT_DRIVES;
    }

    if (general == 1) {
        nr_drives = DEFAULT_DRIVES;
        uae4all_hard_dir[0] = '\0';
        uae4all_hard_file0[0] = '\0';
        uae4all_hard_file1[0] = '\0';
        uae4all_hard_file2[0] = '\0';
        uae4all_hard_file3[0] = '\0';
        uae4all_hard_file_ro[0] = 0;
        uae4all_hard_file_ro[1] = 0;
        uae4all_hard_file_ro[2] = 0;
        uae4all_hard_file_ro[3] = 0;
        strcpy(custom_kickrom, "./kick.rom");
        kickstart = DEFAULT_KICKSTART;
#if defined(__PS3__)
        ps3_set_kickstart(kickstart, 0);
#else
        snprintf(romfile, sizeof(romfile), "%s/kickstarts/%s", launchDir, kickstarts_rom_names[kickstart]);
        FILE *kick_file = fopen(romfile, "rb");
        if (!kick_file) {
            strcpy(romfile, "kick.rom");
        } else {
            fclose(kick_file);
        }
        if (extended_rom_names[kickstart][0] != '\0')
            snprintf(extfile, sizeof(extfile), "%s/kickstarts/%s", launchDir, extended_rom_names[kickstart]);
        else
            extfile[0] = '\0';
#endif
#ifdef __PSP2__
        cdrom_close_image();
#endif
        bReloadKickstart = 1;
    }
    mainMenu_floppyspeed = 100;

    mainMenu_CPU_model = DEFAULT_CPU_MODEL;
    mainMenu_chipset = DEFAULT_CHIPSET_SELECT;
    UpdateCPUModelSettings();
    UpdateChipsetSettings();
    mainMenu_spriteCollisions = DEFAULT_SPRITECOLLISIONS;
    kickstart = DEFAULT_KICKSTART;
    mainMenu_sound = DEFAULT_SOUND;
    sound_rate = DEFAULT_SOUND_FREQ;
    mainMenu_soundStereo = 1;
    mainMenu_soundStereoSep = 4;
    mainMenu_diskSoundVolume = 35;
    disk_sound_set_volume(mainMenu_diskSoundVolume);
    disk_sound_reset();
    mainMenu_CPU_speed = 0;

    mainMenu_cpuSpeed = 600;
    mainMenu_joyConf = 0;
    mainMenu_joyPort = 2;
    mainMenu_autofireRate = 8;
    mainMenu_customAutofireButton = 0;
    mainMenu_showStatus = DEFAULT_STATUSLN;
    mainMenu_background = 0;
    mainMenu_font = 0;
    mainMenu_mouseMultiplier = DEFAULT_MOUSEMULTIPLIER;
    mainMenu_mouseEmulation = 1;
    mainMenu_mouseDevice = 0;
    mainMenu_mouseAcceleration = 0;
    mainMenu_mouseSlowFactor = 2;
    mainMenu_mouseSlowButton = 1;
    mainMenu_mouseFastFactor = 2;
    mainMenu_mouseFastButton = 2;
    mainMenu_mouseSwapButtons = 0;
    mainMenu_stylusOffset = 0;
    mainMenu_tapDelay = 10;
    mainMenu_customControls = 0;
	mainMenu_custom_currentlyEditingControllerNr = 0;
    mainMenu_custom_dpad = 0;
	 for (int i=0; i<MAX_NUM_CONTROLLERS; i++)
	 {
		 mainMenu_custom_up[i] = 0;
		 mainMenu_custom_down[i] = 0;
		 mainMenu_custom_left[i] = 0;
		 mainMenu_custom_right[i] = 0;
		 mainMenu_custom_stickup[i] = 0;
		 mainMenu_custom_stickdown[i] = 0;
		 mainMenu_custom_stickleft[i] = 0;
		 mainMenu_custom_stickright[i] = 0;
		 mainMenu_custom_A[i] = 0;
		 mainMenu_custom_B[i] = 0;
		 mainMenu_custom_X[i] = 0;
		 mainMenu_custom_Y[i] = 0;
		 mainMenu_custom_L[i] = 0;
		 mainMenu_custom_R[i] = 0;
#ifdef __PS3__
		 mainMenu_custom_L2[i] = 0;
		 mainMenu_custom_R2[i] = 0;
		 mainMenu_custom_L3[i] = 0;
		 mainMenu_custom_R3[i] = 0;
#endif
	}
#if defined(__PSP2__) || defined(__PS3__)
	 mainMenu_custom_controlSet = 0;
	 for (int i=0; i<MAX_NUM_CONTROLLERS; i++)
	 {
		 for (int j=0; j<MAX_NUM_CUSTOM_PRESETS; j++)
		 {
			mainMenu_customPreset_up[j][i] = 0;
			mainMenu_customPreset_down[j][i] = 0;
			mainMenu_customPreset_left[j][i] = 0;
			mainMenu_customPreset_right[j][i] = 0;
			mainMenu_customPreset_stickup[j][i] = 0;
			mainMenu_customPreset_stickdown[j][i] = 0;
			mainMenu_customPreset_stickleft[j][i] = 0;
			mainMenu_customPreset_stickright[j][i] = 0;
			mainMenu_customPreset_A[j][i] = 0;
			mainMenu_customPreset_B[j][i] = 0;
			mainMenu_customPreset_X[j][i] = 0;
			mainMenu_customPreset_Y[j][i] = 0;
			mainMenu_customPreset_L[j][i] = 0;
			mainMenu_customPreset_R[j][i] = 0;
#ifdef __PS3__
			mainMenu_customPreset_L2[j][i] = 0;
			mainMenu_customPreset_R2[j][i] = 0;
			mainMenu_customPreset_L3[j][i] = 0;
			mainMenu_customPreset_R3[j][i] = 0;
#endif
		}
	}
	mainMenu_customPreset_up[0][0] = -5;
	mainMenu_customPreset_down[0][0] = -6;
	mainMenu_customPreset_left[0][0] = -7;
	mainMenu_customPreset_right[0][0] = -8;
	mainMenu_customPreset_stickup[0][0] = -5;
	mainMenu_customPreset_stickdown[0][0] = -6;
	mainMenu_customPreset_stickleft[0][0] = -7;
	mainMenu_customPreset_stickright[0][0] = -8;
	mainMenu_customPreset_X[0][0] = -5;
	mainMenu_customPreset_B[0][0] = 23;
	mainMenu_customPreset_A[0][0] = -3;
	mainMenu_customPreset_Y[0][0] = -4;
	mainMenu_customPreset_L[0][0] = -2;
	mainMenu_customPreset_R[0][0] = -1;
#ifdef __PS3__
	mainMenu_customPreset_L2[0][0] = -26;
	mainMenu_customPreset_R2[0][0] = -25;
	mainMenu_customPreset_L3[0][0] = 0;
	mainMenu_customPreset_R3[0][0] = 0;
#endif
	mainMenu_customPreset_up[0][1] = -9;
	mainMenu_customPreset_down[0][1] = -10;
	mainMenu_customPreset_left[0][1] = -11;
	mainMenu_customPreset_right[0][1] = -12;
	mainMenu_customPreset_stickup[0][1] = -9;
	mainMenu_customPreset_stickdown[0][1] = -10;
	mainMenu_customPreset_stickleft[0][1] = -11;
	mainMenu_customPreset_stickright[0][1] = -12;
	mainMenu_customPreset_X[0][1] = -9;
	mainMenu_customPreset_B[0][1] = 0;
	mainMenu_customPreset_A[0][1] = -1;
	mainMenu_customPreset_Y[0][1] = -2;
	mainMenu_customPreset_L[0][1] = 0;
	mainMenu_customPreset_R[0][1] = 0;
#ifdef __PS3__
	mainMenu_customPreset_L2[0][1] = 0;
	mainMenu_customPreset_R2[0][1] = 0;
	mainMenu_customPreset_L3[0][1] = 0;
	mainMenu_customPreset_R3[0][1] = 0;
#endif
	mainMenu_customPreset_up[0][2] = -15;
	mainMenu_customPreset_down[0][2] = -16;
	mainMenu_customPreset_left[0][2] = -17;
	mainMenu_customPreset_right[0][2] = -18;
	mainMenu_customPreset_stickup[0][2] = -15;
	mainMenu_customPreset_stickdown[0][2] = -16;
	mainMenu_customPreset_stickleft[0][2] = -17;
	mainMenu_customPreset_stickright[0][2] = -18;
	mainMenu_customPreset_X[0][2] = -15;
	mainMenu_customPreset_B[0][2] = 0;
	mainMenu_customPreset_A[0][2] = -13;
	mainMenu_customPreset_Y[0][2] = -14;
	mainMenu_customPreset_L[0][2] = 0;
	mainMenu_customPreset_R[0][2] = 0;
#ifdef __PS3__
	mainMenu_customPreset_L2[0][2] = 0;
	mainMenu_customPreset_R2[0][2] = 0;
	mainMenu_customPreset_L3[0][2] = 0;
	mainMenu_customPreset_R3[0][2] = 0;
#endif
	mainMenu_customPreset_up[0][3] = -21;
	mainMenu_customPreset_down[0][3] = -22;
	mainMenu_customPreset_left[0][3] = -23;
	mainMenu_customPreset_right[0][3] = -24;
	mainMenu_customPreset_stickup[0][3] = -21;
	mainMenu_customPreset_stickdown[0][3] = -22;
	mainMenu_customPreset_stickleft[0][3] = -23;
	mainMenu_customPreset_stickright[0][3] = -24;
	mainMenu_customPreset_X[0][3] = -21;
	mainMenu_customPreset_B[0][3] = 0;
	mainMenu_customPreset_A[0][3] = -19;
	mainMenu_customPreset_Y[0][3] = -20;
	mainMenu_customPreset_L[0][3] = 0;
	mainMenu_customPreset_R[0][3] = 0;
#ifdef __PS3__
	mainMenu_customPreset_L2[0][3] = 0;
	mainMenu_customPreset_R2[0][3] = 0;
	mainMenu_customPreset_L3[0][3] = 0;
	mainMenu_customPreset_R3[0][3] = 0;
#endif

	mainMenu_customPreset_up[1][0] = 0;
	mainMenu_customPreset_down[1][0] = 2;
	mainMenu_customPreset_left[1][0] = 33;
	mainMenu_customPreset_right[1][0] = 30;
	mainMenu_customPreset_stickup[1][0] = 0;
	mainMenu_customPreset_stickdown[1][0] = 2;
	mainMenu_customPreset_stickleft[1][0] = 33;
	mainMenu_customPreset_stickright[1][0] = 30;
	mainMenu_customPreset_X[1][0] = 23;
	mainMenu_customPreset_B[1][0] = 34;
	mainMenu_customPreset_A[1][0] = 26;
	mainMenu_customPreset_Y[1][0] = 87;
	mainMenu_customPreset_L[1][0] = 33;
	mainMenu_customPreset_R[1][0] = 34;
#ifdef __PS3__
	mainMenu_customPreset_L2[1][0] = 33;
	mainMenu_customPreset_R2[1][0] = 34;
	mainMenu_customPreset_L3[1][0] = 0;
	mainMenu_customPreset_R3[1][0] = 0;
#endif

	mainMenu_customPreset_up[1][1] = 0;
	mainMenu_customPreset_down[1][1] = 2;
	mainMenu_customPreset_left[1][1] = 33;
	mainMenu_customPreset_right[1][1] = 30;
	mainMenu_customPreset_stickup[1][1] = 0;
	mainMenu_customPreset_stickdown[1][1] = 2;
	mainMenu_customPreset_stickleft[1][1] = 33;
	mainMenu_customPreset_stickright[1][1] = 30;
	mainMenu_customPreset_X[1][1] = 23;
	mainMenu_customPreset_B[1][1] = 34;
	mainMenu_customPreset_A[1][1] = 26;
	mainMenu_customPreset_Y[1][1] = 88;
	mainMenu_customPreset_L[1][1] = 33;
	mainMenu_customPreset_R[1][1] = 34;
#ifdef __PS3__
	mainMenu_customPreset_L2[1][1] = 33;
	mainMenu_customPreset_R2[1][1] = 34;
	mainMenu_customPreset_L3[1][1] = 0;
	mainMenu_customPreset_R3[1][1] = 0;
#endif

	mainMenu_customPreset_up[1][2] = 0;
	mainMenu_customPreset_down[1][2] = 2;
	mainMenu_customPreset_left[1][2] = 33;
	mainMenu_customPreset_right[1][2] = 30;
	mainMenu_customPreset_stickup[1][2] = 0;
	mainMenu_customPreset_stickdown[1][2] = 2;
	mainMenu_customPreset_stickleft[1][2] = 33;
	mainMenu_customPreset_stickright[1][2] = 30;
	mainMenu_customPreset_X[1][2] = 23;
	mainMenu_customPreset_B[1][2] = 34;
	mainMenu_customPreset_A[1][2] = 26;
	mainMenu_customPreset_Y[1][2] = 89;
	mainMenu_customPreset_L[1][2] = 33;
	mainMenu_customPreset_R[1][2] = 34;
#ifdef __PS3__
	mainMenu_customPreset_L2[1][2] = 33;
	mainMenu_customPreset_R2[1][2] = 34;
	mainMenu_customPreset_L3[1][2] = 0;
	mainMenu_customPreset_R3[1][2] = 0;
#endif

	mainMenu_customPreset_up[1][3] = 0;
	mainMenu_customPreset_down[1][3] = 2;
	mainMenu_customPreset_left[1][3] = 33;
	mainMenu_customPreset_right[1][3] = 30;
	mainMenu_customPreset_stickup[1][3] = 0;
	mainMenu_customPreset_stickdown[1][3] = 2;
	mainMenu_customPreset_stickleft[1][3] = 33;
	mainMenu_customPreset_stickright[1][3] = 30;
	mainMenu_customPreset_X[1][3] = 23;
	mainMenu_customPreset_B[1][3] = 34;
	mainMenu_customPreset_A[1][3] = 26;
	mainMenu_customPreset_Y[1][3] = 90;
	mainMenu_customPreset_L[1][3] = 33;
	mainMenu_customPreset_R[1][3] = 34;
#ifdef __PS3__
	mainMenu_customPreset_L2[1][3] = 33;
	mainMenu_customPreset_R2[1][3] = 34;
	mainMenu_customPreset_L3[1][3] = 0;
	mainMenu_customPreset_R3[1][3] = 0;
#endif
	remap_custom_controls();
#endif
    SetPresetMode(2);
    moveX = 0;
    moveY = 16;
    mainMenu_cutLeft = 0;
    mainMenu_cutRight = 0;
    mainMenu_footerSize = 0;
    mainMenu_screenOffsetY = 0;
    mainMenu_screenOffsetX = 0;
    mainMenu_autoCrop = 0;
    mainMenu_ntsc = DEFAULT_NTSC;
    mainMenu_frameskip = 0;
    mainMenu_vkbdLanguage = 0;
    mainMenu_vkbdStyle = 0;
    mainMenu_vkbdTransparency = 1;
    mainMenu_vkbdPosition = 0;
    mainMenu_autofire = 0;
    mainMenu_autofireMode = 0;

#if defined(__PSP2__) || defined(__PS3__)
#ifdef __PS3__
    mainMenu_shader = 0;
#else
    mainMenu_shader = 5;
#endif
    mainMenu_leftStickMouse = 0;
    mainMenu_touchControls = 1;
    mainMenu_deadZone = 1000;
    mainMenu_autoEjectFloppy = 1;
    mainMenu_midiSynth = 0;
    mainMenu_pinballMode = 0;
    mainMenu_whdload_game[0] = '\0';
#endif
#ifdef __PS3__
    mainMenu_swapAB = DEFAULT_SWAPAB;
    mainMenu_singleJoycons = DEFAULT_SINGLEJOYCONS;
    mainMenu_numPlayers = 1;
#endif
    skipintro = DEFAULT_SKIPINTRO;
    mainMenu_autosave = DEFAULT_AUTOSAVE;
    mainMenu_button1 = GP2X_BUTTON_X;
    mainMenu_button2 = GP2X_BUTTON_A;
    mainMenu_autofireButton1 = GP2X_BUTTON_B;
    mainMenu_jump = -1;

    gp2xClockSpeed = -1;
    mainMenu_scanlines = 0;
    mainMenu_enableScreenshots = DEFAULT_ENABLESCREENSHOTS;
    mainMenu_enableScripts = DEFAULT_ENABLESCRIPTS;
    mainMenu_useSavesFolder = DEFAULT_USESAVESFOLDER;
}


void UpdateCPUModelSettings()
{
    switch (mainMenu_CPU_model) {
    case 1:
        changed_prefs.cpu_level = M68020;
        break;
    default:
        changed_prefs.cpu_level = M68000;
        break;
    }
}


void UpdateMemorySettings()
{
    prefs_chipmem_size = 0x000080000 << mainMenu_chipMemory;

    if ((mainMenu_chipMemory > 2) && (mainMenu_fastMemory > 0)) {
        mainMenu_fastMemory = 0;
        changed_prefs.fastmem_size = 0;
    }

    switch (mainMenu_slowMemory) {
    case 1:
    case 2:
        prefs_bogomem_size = 0x00080000 << (mainMenu_slowMemory - 1);
        break;
    case 3:
        prefs_bogomem_size = 0x00180000;
        break;
    case 4:
        prefs_bogomem_size = 0x001C0000;
        break;
    default:
        prefs_bogomem_size = 0;
        break;
    }

    if (mainMenu_rtgMemory > 0) {
        if (mainMenu_fastMemory > 2) {
            mainMenu_fastMemory = 2;
        }
    }

    switch (mainMenu_fastMemory) {
    case 0:
        changed_prefs.fastmem_size = 0;
        break;
    default:
        changed_prefs.fastmem_size = 0x00080000 << mainMenu_fastMemory;
    }

    switch (mainMenu_rtgMemory) {
    case 1:
        changed_prefs.gfxmem_size = 0x200000;
        break;
    case 2:
        changed_prefs.gfxmem_size = 0x400000;
        break;
    default:
        changed_prefs.gfxmem_size = 0;
        break;
    }
}


static int has_hdf_files(void)
{
    return uae4all_hard_file0[0] || uae4all_hard_file1[0] ||
           uae4all_hard_file2[0] || uae4all_hard_file3[0];
}

void ApplyA500Profile(void)
{
    cdrom_close_image();
    cdrom_set_cd32_mode(0);
    kickstart = 1;
    extfile[0] = '\0';
    mainMenu_CPU_model = 0;
    mainMenu_chipset = 0x100;
    mainMenu_chipMemory = 0;
    mainMenu_slowMemory = 1;
    mainMenu_fastMemory = 0;
    mainMenu_rtgMemory = 0;
    mainMenu_bootHD = 0;
    mainMenu_autoCrop = 0;
    if (mainMenu_drives < 1)
        mainMenu_drives = DEFAULT_DRIVES;
    UpdateCPUModelSettings();
    UpdateMemorySettings();
    UpdateChipsetSettings();
    reset_hdConf();
    bReloadKickstart = 1;
#if defined(__PS3__)
    ps3_set_kickstart(kickstart, 0);
#endif
}

void ApplyA1200Profile(void)
{
    cdrom_close_image();
    cdrom_set_cd32_mode(0);
    kickstart = 3;
    extfile[0] = '\0';
    mainMenu_CPU_model = 1;
    mainMenu_chipset = 2 | 0x100;
    mainMenu_chipMemory = 2;
    mainMenu_slowMemory = 0;
    mainMenu_fastMemory = (mainMenu_rtgMemory > 0) ? 2 : 3;
    mainMenu_bootHD = has_hdf_files() ? 2 : 0;
    mainMenu_autoCrop = 0;
    if (mainMenu_drives < 1)
        mainMenu_drives = DEFAULT_DRIVES;
    UpdateCPUModelSettings();
    UpdateMemorySettings();
    UpdateChipsetSettings();
    reset_hdConf();
    bReloadKickstart = 1;
#if defined(__PS3__)
    ps3_set_kickstart(kickstart, 0);
#endif
}

void ApplyCd32Profile(void)
{
    disk_sound_reset();
    cdrom_set_cd32_mode(1);
    kickstart = 6;
    mainMenu_CPU_model = 1;
    mainMenu_chipset = 2;
    mainMenu_chipMemory = 2;
    mainMenu_fastMemory = 0;
    mainMenu_slowMemory = 0;
    mainMenu_rtgMemory = 0;
    mainMenu_bootHD = 0;
    mainMenu_drives = 1;
    UpdateCPUModelSettings();
    UpdateMemorySettings();
    UpdateChipsetSettings();
    reset_hdConf();
    bReloadKickstart = 1;
#if defined(__PS3__)
    ps3_set_kickstart(kickstart, 0);
#endif
}

void ApplyWHDLoadA500Profile(void)
{
    cdrom_close_image();
    cdrom_set_cd32_mode(0);
    kickstart = 3;
    extfile[0] = '\0';
    mainMenu_CPU_model = 1;
    mainMenu_chipset = 2 | 0x100;
    mainMenu_chipMemory = 2;
    mainMenu_slowMemory = 0;
    mainMenu_fastMemory = 3;
    mainMenu_rtgMemory = 0;
    mainMenu_bootHD = 1;
    UpdateCPUModelSettings();
    UpdateMemorySettings();
    UpdateChipsetSettings();
    reset_hdConf();
    bReloadKickstart = 1;
#if defined(__PS3__)
    ps3_set_kickstart(kickstart, 0);
#endif
}

void ApplyWHDLoadPreset(const char *game_name)
{
    mainMenu_rtgMemory = 0;
    int is_aga = 0;
    if (mainMenu_whdload_mode == 1) {
        is_aga = 0;
    } else if (mainMenu_whdload_mode == 2) {
        is_aga = 1;
    } else {
#ifdef __PS3__
        is_aga = ps3_whdload_is_aga(game_name);
#else
        is_aga = 0;
#endif
    }

    if (is_aga) {
        ApplyA1200Profile();
    } else {
        ApplyWHDLoadA500Profile();
    }

    int blit_mode = 0x100;
    if (mainMenu_whdload_blitter == 1) {
        blit_mode = 0x000;
    } else if (mainMenu_whdload_blitter == 2) {
        blit_mode = 0x100;
    } else if (mainMenu_whdload_blitter == 3) {
        blit_mode = 0x200;
    } else {
#ifdef __PS3__
        if (ps3_whdload_needs_improved_blitter(game_name)) {
            blit_mode = 0x200;
        } else {
            blit_mode = 0x100;
        }
#else
        blit_mode = 0x100;
#endif
    }
    mainMenu_chipset = (mainMenu_chipset & 0x00ff) | blit_mode;
    UpdateChipsetSettings();

    if (uae4all_hard_dir[0] != '\0') {
        mainMenu_bootHD = 1;
        reset_hdConf();
    }
#if defined(__PS3__)
    ps3_set_kickstart(kickstart, 0);
#endif
}

void ApplyAutomaticGamePreset(int media_type)
{
    if (media_type != 3)
        cdrom_set_cd32_mode(0);
    if (media_type == 3) {
        ApplyCd32Profile();
    } else if (media_type == 2) {
        ApplyWHDLoadPreset(mainMenu_whdload_game[0] ? mainMenu_whdload_game : NULL);
    } else if (media_type == 1) {
        ApplyA1200Profile();
    } else {
        ApplyA500Profile();
    }
#if defined(__PS3__)
    ps3_set_kickstart(kickstart, 0);
#endif
}

void UpdateChipsetSettings()
{
    switch (mainMenu_chipset & 0xff) {
    case 1:
        changed_prefs.chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
        break;
    case 2:
        changed_prefs.chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE | CSMASK_AGA;
        break;
    default:
        /* OCS: If chip RAM > 512KB, fat Agnus ECS is required for address space */
        if (prefs_chipmem_size > 512 * 1024)
            changed_prefs.chipset_mask = CSMASK_ECS_AGNUS;
        else
            changed_prefs.chipset_mask = 0;
        break;
    }
    switch (mainMenu_chipset & 0xff00) {
    case 0x100:
        changed_prefs.immediate_blits = mainMenu_cycleExact ? false : true;
        blitter_in_partial_mode = 0;
        break;
    case 0x200:
        changed_prefs.immediate_blits = false;
        blitter_in_partial_mode = mainMenu_cycleExact ? 0 : 1;
        break;
    default:
        changed_prefs.immediate_blits = false;
        blitter_in_partial_mode = 0;
        break;
    }
    if (mainMenu_cycleExact) {
        changed_prefs.immediate_blits = false;
        blitter_in_partial_mode = 0;
        mainMenu_spriteCollisions = 1;
    }
}


void SetPresetMode(int mode)
{
    presetModeId = mode;

    switch(mode) {
    case 0:
        mainMenu_displayedLines = 200;
        screenWidth = 768;
        strcpy(presetMode, "320x200 upscaled");
        break;

    case 1:
        mainMenu_displayedLines = 216;
        screenWidth = 716;
        strcpy(presetMode, "320x216 upscaled");
        break;

    case 2:
        mainMenu_displayedLines = 240;
        screenWidth = 640;
        strcpy(presetMode, "320x240 upscaled");
        break;

    case 3:
        mainMenu_displayedLines = 256;
        screenWidth = 600;
        strcpy(presetMode, "320x256 upscaled");
        break;

    case 4:
        mainMenu_displayedLines = 262;
        screenWidth = 588;
        strcpy(presetMode, "320x262 upscaled");
        break;

    case 5:
        mainMenu_displayedLines = 270;
        screenWidth = 570;
        strcpy(presetMode, "320x270 upscaled");
        break;

    case 6:
        mainMenu_displayedLines = 200;
        screenWidth = 640;
        strcpy(presetMode, "320x200 NTSC");
        break;

    case 7:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "320x200 fullscreen");
        break;

    case 8:
        mainMenu_displayedLines = 256;
        screenWidth = 600;
        strcpy(presetMode, "320x256 5:4");
        break;

    case 9:
        mainMenu_displayedLines = 240;
        screenWidth = 640;
        strcpy(presetMode, "320x240 16:10");
        break;

    case 10:
        mainMenu_displayedLines = 200;
        screenWidth = 768;
        strcpy(presetMode, "640x200 upscaled");
        break;

    case 11:
        mainMenu_displayedLines = 216;
        screenWidth = 716;
        strcpy(presetMode, "640x216 upscaled");
        break;

    case 12:
        mainMenu_displayedLines = 240;
        screenWidth = 640;
        strcpy(presetMode, "640x240 upscaled");
        break;

    case 13:
        mainMenu_displayedLines = 256;
        screenWidth = 600;
        strcpy(presetMode, "640x256 upscaled");
        break;

    case 14:
        mainMenu_displayedLines = 262;
        screenWidth = 588;
        strcpy(presetMode, "640x262 upscaled");
        break;

    case 15:
        mainMenu_displayedLines = 270;
        screenWidth = 570;
        strcpy(presetMode, "640x270 upscaled");
        break;

    case 16:
        mainMenu_displayedLines = 200;
        screenWidth = 640;
        strcpy(presetMode, "640x200 NTSC");
        break;

    case 17:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "640x200 fullscreen");
        break;

    case 18:
        mainMenu_displayedLines = 256;
        screenWidth = 600;
        strcpy(presetMode, "640x256 5:4");
        break;

    case 19:
        mainMenu_displayedLines = 240;
        screenWidth = 640;
        strcpy(presetMode, "640x240 16:10");
        break;

    case 20:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "352x200 upscaled");
        break;

    case 21:
        mainMenu_displayedLines = 216;
        screenWidth = 784;
        strcpy(presetMode, "352x216 upscaled");
        break;

    case 22:
        mainMenu_displayedLines = 240;
        screenWidth = 704;
        strcpy(presetMode, "352x240 upscaled");
        break;

    case 23:
        mainMenu_displayedLines = 256;
        screenWidth = 660;
        strcpy(presetMode, "352x256 upscaled");
        break;

    case 24:
        mainMenu_displayedLines = 262;
        screenWidth = 640;
        strcpy(presetMode, "352x262 upscaled");
        break;

    case 25:
        mainMenu_displayedLines = 270;
        screenWidth = 624;
        strcpy(presetMode, "352x270 upscaled");
        break;

    case 26:
        mainMenu_displayedLines = 200;
        screenWidth = 704;
        strcpy(presetMode, "352x200 NTSC");
        break;

    case 27:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "352x200 fullscreen");
        break;

    case 28:
        mainMenu_displayedLines = 256;
        screenWidth = 660;
        strcpy(presetMode, "352x256 5:4");
        break;

    case 29:
        mainMenu_displayedLines = 240;
        screenWidth = 704;
        strcpy(presetMode, "352x240 16:10");
        break;

    case 30:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "704x200 upscaled");
        break;

    case 31:
        mainMenu_displayedLines = 216;
        screenWidth = 784;
        strcpy(presetMode, "704x216 upscaled");
        break;

    case 32:
        mainMenu_displayedLines = 240;
        screenWidth = 704;
        strcpy(presetMode, "704x240 upscaled");
        break;

    case 33:
        mainMenu_displayedLines = 256;
        screenWidth = 660;
        strcpy(presetMode, "704x256 upscaled");
        break;

    case 34:
        mainMenu_displayedLines = 262;
        screenWidth = 640;
        strcpy(presetMode, "704x262 upscaled");
        break;

    case 35:
        mainMenu_displayedLines = 270;
        screenWidth = 624;
        strcpy(presetMode, "704x270 upscaled");
        break;

    case 36:
        mainMenu_displayedLines = 200;
        screenWidth = 704;
        strcpy(presetMode, "704x200 NTSC");
        break;

    case 37:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "704x200 fullscreen");
        break;

    case 38:
        mainMenu_displayedLines = 256;
        screenWidth = 660;
        strcpy(presetMode, "704x256 5:4");
        break;

    case 39:
        mainMenu_displayedLines = 240;
        screenWidth = 704;
        strcpy(presetMode, "704x240 16:10");
        break;

    case 40:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "384x200 upscaled");
        break;

    case 41:
        mainMenu_displayedLines = 216;
        screenWidth = 800;
        strcpy(presetMode, "384x216 upscaled");
        break;

    case 42:
        mainMenu_displayedLines = 240;
        screenWidth = 768;
        strcpy(presetMode, "384x240 upscaled");
        break;

    case 43:
        mainMenu_displayedLines = 256;
        screenWidth = 720;
        strcpy(presetMode, "384x256 upscaled");
        break;

    case 44:
        mainMenu_displayedLines = 262;
        screenWidth = 704;
        strcpy(presetMode, "384x262 upscaled");
        break;

    case 45:
        mainMenu_displayedLines = 270;
        screenWidth = 684;
        strcpy(presetMode, "384x270 upscaled");
        break;

    case 46:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "384x200 NTSC");
        break;

    case 47:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "384x200 fullscreen");
        break;

    case 48:
        mainMenu_displayedLines = 256;
        screenWidth = 720;
        strcpy(presetMode, "384x256 5:4");
        break;

    case 49:
        mainMenu_displayedLines = 240;
        screenWidth = 768;
        strcpy(presetMode, "384x240 16:10");
        break;

    case 50:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "768x200 upscaled");
        break;

    case 51:
        mainMenu_displayedLines = 216;
        screenWidth = 800;
        strcpy(presetMode, "768x216 upscaled");
        break;

    case 52:
        mainMenu_displayedLines = 240;
        screenWidth = 768;
        strcpy(presetMode, "768x240 upscaled");
        break;

    case 53:
        mainMenu_displayedLines = 256;
        screenWidth = 720;
        strcpy(presetMode, "768x256 upscaled");
        break;

    case 54:
        mainMenu_displayedLines = 262;
        screenWidth = 704;
        strcpy(presetMode, "768x262 upscaled");
        break;

    case 55:
        mainMenu_displayedLines = 270;
        screenWidth = 684;
        strcpy(presetMode, "768x270 upscaled");
        break;

    case 56:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "768x200 NTSC");
        break;

    case 57:
        mainMenu_displayedLines = 200;
        screenWidth = 800;
        strcpy(presetMode, "768x200 fullscreen");
        break;

    case 58:
        mainMenu_displayedLines = 256;
        screenWidth = 720;
        strcpy(presetMode, "768x256 5:4");
        break;

    case 59:
        mainMenu_displayedLines = 240;
        screenWidth = 768;
        strcpy(presetMode, "768x240 16:10");
        break;

    default:
        mainMenu_displayedLines = 240;
        screenWidth = 640;
        strcpy(presetMode, "320x240 upscaled");
        presetModeId = 2;
        break;
    }

    switch(presetModeId / 10) {
    case 0:
        mainMenu_displayHires = 0;
        visibleAreaWidth = 320;
        break;

    case 1:
        mainMenu_displayHires = 1;
        visibleAreaWidth = 640;
        break;

    case 2:
        mainMenu_displayHires = 0;
        visibleAreaWidth = 352;
        break;

    case 3:
        mainMenu_displayHires = 1;
        visibleAreaWidth = 704;
        break;

    case 4:
        mainMenu_displayHires = 0;
        visibleAreaWidth = 384;
        break;

    case 5:
        mainMenu_displayHires = 1;
        visibleAreaWidth = 768;
        break;
    }
}


void set_joyConf()
{
    if(mainMenu_joyConf==0) {
        mainMenu_button1=GP2X_BUTTON_X;
        mainMenu_button2=GP2X_BUTTON_A;
        mainMenu_jump=-1;
        mainMenu_autofireButton1=GP2X_BUTTON_B;
    } else if(mainMenu_joyConf==1) {
        mainMenu_button1=GP2X_BUTTON_B;
        mainMenu_button2=GP2X_BUTTON_A;
        mainMenu_jump=-1;
        mainMenu_autofireButton1=GP2X_BUTTON_X;
    } else if(mainMenu_joyConf==2) {
        mainMenu_button1=GP2X_BUTTON_Y;
        mainMenu_button2=GP2X_BUTTON_A;
        mainMenu_jump=GP2X_BUTTON_X;
        mainMenu_autofireButton1=GP2X_BUTTON_B;
    } else if(mainMenu_joyConf==3) {
        mainMenu_button1=GP2X_BUTTON_B;
        mainMenu_button2=GP2X_BUTTON_A;
        mainMenu_jump=GP2X_BUTTON_X;
        mainMenu_autofireButton1=GP2X_BUTTON_Y;
    }
}

void kill_hd_configs()
{
	if (currprefs.mountinfo) {
		while (nr_units(currprefs.mountinfo) > 0) {
			kill_filesys_unit(currprefs.mountinfo, 0);
		}
	}
	mainMenu_filesysUnits = 0;
}

void reset_hdConf()
{
	 kill_hd_configs();

    /* One line in the runtime log per reset: what the machine is supposed to
     * boot. A failed/missing DH0 mount is the #1 reason a WHDLoad game
     * "does not start", and without this line the log cannot say so. */
    write_log("[HD] reset_hdConf: bootHD=%d dir='%s' file0='%s' file1='%s'\n",
              mainMenu_bootHD,
              uae4all_hard_dir,
              mainMenu_bootHD ? uae4all_hard_file0 : "",
              mainMenu_bootHD ? uae4all_hard_file1 : "");

    switch (mainMenu_bootHD) {
    case 0:
        break;
    case 1:
        if (uae4all_hard_dir[0] != '\0') {
            parse_filesys_spec(0, uae4all_hard_dir);
            mainMenu_filesysUnits++;
            write_log("[HD] mounted hard dir as DH0: '%s' (units=%d)\n",
                      uae4all_hard_dir, mainMenu_filesysUnits);
        }
        if (uae4all_hard_file0[0] != '\0') {
            /* An RDB image mounts one unit per partition. */
            mainMenu_filesysUnits += parse_hardfile_spec(uae4all_hard_file_ro[0], uae4all_hard_file0);
        }
        if (uae4all_hard_file1[0] != '\0') {
            mainMenu_filesysUnits += parse_hardfile_spec(uae4all_hard_file_ro[1], uae4all_hard_file1);
        }
        if (uae4all_hard_file2[0] != '\0') {
            mainMenu_filesysUnits += parse_hardfile_spec(uae4all_hard_file_ro[2], uae4all_hard_file2);
        }
        if (uae4all_hard_file3[0] != '\0') {
            mainMenu_filesysUnits += parse_hardfile_spec(uae4all_hard_file_ro[3], uae4all_hard_file3);
        }
        break;
    case 2:
        if (uae4all_hard_file0[0] != '\0') {
            /* An RDB image mounts one unit per partition. */
            mainMenu_filesysUnits += parse_hardfile_spec(uae4all_hard_file_ro[0], uae4all_hard_file0);
        }
        if (uae4all_hard_file1[0] != '\0') {
            mainMenu_filesysUnits += parse_hardfile_spec(uae4all_hard_file_ro[1], uae4all_hard_file1);
        }
        if (uae4all_hard_file2[0] != '\0') {
            mainMenu_filesysUnits += parse_hardfile_spec(uae4all_hard_file_ro[2], uae4all_hard_file2);
        }
        if (uae4all_hard_file3[0] != '\0') {
            mainMenu_filesysUnits += parse_hardfile_spec(uae4all_hard_file_ro[3], uae4all_hard_file3);
        }
        if (uae4all_hard_dir[0] != '\0') {
            parse_filesys_spec(0, uae4all_hard_dir);
            mainMenu_filesysUnits++;
        }
        break;
    }
}


static void replace(char * str,char replace, char toreplace)
{
    while(*str) {
        if (*str==toreplace) *str=replace;
        str++;
    }
}


int create_configfilename(char *dest, char *basename, int fromDir)
{
    if (!dest || !basename || basename[0] == '\0') return 0;

    const char *p = strrchr(basename, '/');
    if (!p) p = strrchr(basename, '\\');
    if (p) p++;
    else p = basename;

    char filename[256];
    strncpy(filename, p, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';

    if (fromDir == 0) {
        char *pch = strrchr(filename, '.');
        if (pch) *pch = '\0';
    }

    const char *base_dir = (launchDir[0] != '\0') ? launchDir : "ux0:/data/uae4all";
    snprintf(dest, 300, "%s/conf/%s.conf", base_dir, filename);
    return 1;
}


const char *config_save_as_name = NULL;

int saveconfig(int general)
{
    char path[300];
#ifdef __PS3__
    const char *base_dir = (launchDir[0] != '\0') ? launchDir : "./data";
    mkdir("./data", 0777);
    mkdir("./data/conf", 0777);
    mkdir("./tmp", 0777);
#else
    const char *base_dir = (launchDir[0] != '\0') ? launchDir : "ux0:/data/uae4all";
    mkdir("ux0:/data/uae4all", 0777);
    mkdir("ux0:/data/uae4all/conf", 0777);
#endif

    switch(general) {
    case 0:
        if (!uae4all_image_file0[0]) {
            snprintf(path, 300, "%s/conf/default.conf", base_dir);
        } else {
            if(!create_configfilename(path, uae4all_image_file0, 0))
                snprintf(path, 300, "%s/conf/default.conf", base_dir);
        }
        break;
    case 1:
        snprintf(path, 300, "%s/conf/uaeconfig.conf", base_dir);
        break;
    case 2:
        path[0] = '\0';
        if(mainMenu_bootHD == 1 && uae4all_hard_dir[0] != '\0')
            create_configfilename(path, uae4all_hard_dir, 1);
        if(mainMenu_bootHD == 2 && uae4all_hard_file0[0] != '\0')
            create_configfilename(path, uae4all_hard_file0, 0);
        if(path[0] == '\0')
            snprintf(path, 300, "%s/conf/uaeconfig.conf", base_dir);
        break;
    case 3:
        snprintf(path, 300, config_filename, base_dir);
        break;
    case 4:
#if defined(__PS3__) || defined(__PSP2__)
        char buf[100] = "";
#ifdef __PS3__
        if (config_save_as_name && config_save_as_name[0] != '\0') {
            strncpy(buf, config_save_as_name, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
        } else {
            ps3_kbd_get("Enter config name:", "", 100, 0, buf);
        }
#else
        if (config_save_as_name && config_save_as_name[0] != '\0') {
            strncpy(buf, config_save_as_name, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
        } else {
            char *ime_name = kbdvita_get("Enter config name:", "", 100, 0);
            if (!ime_name || ime_name[0] == '\0')
                return 0;
            strncpy(buf, ime_name, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
        }
#endif
        if (buf[0] == 0)
            return 0;
        snprintf(path, 300, "%s/conf/%s.conf", base_dir, buf);
#else
        return 0;
#endif
        break;
    }


    FILE *f=fopen(path,"w");
    if (!f) return 0;
    char buffer[255];

    snprintf((char*)buffer, 255, "kickstart=%d\n",kickstart);
    fputs(buffer,f);
#if defined(PANDORA) || defined(ANDROIDSDL)
    snprintf((char*)buffer, 255, "scaling=%d\n",0);
#else
    snprintf((char*)buffer, 255, "scaling=%d\n",mainMenu_enableHWscaling);
#endif
    fputs(buffer,f);
#if defined(__PSP2__) || defined(__PS3__)
    snprintf((char*)buffer, 255, "shader=%d\n",mainMenu_shader);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "leftstickmouse=%d\n",mainMenu_leftStickMouse);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "touchcontrols=%d\n",mainMenu_touchControls);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "deadzone=%d\n",mainMenu_deadZone);
    fputs(buffer,f);
#endif
    snprintf((char*)buffer, 255, "showstatus=%d\n",mainMenu_showStatus);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "background=%d\n",mainMenu_background);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "font=%d\n",mainMenu_font);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mousemultiplier=%d\n",mainMenu_mouseMultiplier);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mouseemulation=%d\n",mainMenu_mouseEmulation);
    fputs(buffer,f);
#if defined(PANDORA) || defined(ANDROIDSDL)
    snprintf((char*)buffer, 255, "systemclock=%d\n",5);
#else
    snprintf((char*)buffer, 255, "systemclock=%d\n",mainMenu_throttle);
#endif
    fputs(buffer,f);
#if defined(PANDORA) || defined(ANDROIDSDL)
    snprintf((char*)buffer, 255, "syncthreshold=%d\n",2);
#else
    snprintf((char*)buffer, 255, "syncthreshold=%d\n",timeslice_mode);
#endif
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "frameskip=%d\n",mainMenu_frameskip);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "vkbdlanguage=%d\n",mainMenu_vkbdLanguage);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "vkbdstyle=%d\n",mainMenu_vkbdStyle);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "vkbdtransparency=%d\n",mainMenu_vkbdTransparency);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "vkbdposition=%d\n",mainMenu_vkbdPosition);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "sound=%d\n",mainMenu_sound + mainMenu_soundStereo * 10);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "soundstereosep=%d\n",mainMenu_soundStereoSep);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "soundrate=%d\n",sound_rate);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "autosave=%d\n",mainMenu_autosave);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "gp2xclock=%d\n",gp2xClockSpeed);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "joyconf=%d\n",mainMenu_joyConf + (mainMenu_joyPort << 4));
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "autofireRate=%d\n",mainMenu_autofireRate);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "autofire=%d\n",mainMenu_autofire);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "autofireMode=%d\n",mainMenu_autofireMode);
    fputs(buffer,f);
#if defined(__PSP2__) || defined(__PS3__)
    snprintf((char*)buffer, 255, "whdload_args=%s\n",mainMenu_whdload_args);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "whdload_mode=%d\n",mainMenu_whdload_mode);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "whdload_blitter=%d\n",mainMenu_whdload_blitter);
    fputs(buffer,f);
    for (int i = 0; i < 4; i++) {
        snprintf((char*)buffer, 255, "floppyWriteProtect%d=%d\n", i, mainMenu_floppyWriteProtect[i]);
        fputs(buffer, f);
    }
    snprintf((char*)buffer, 255, "cycleExact=%d\n", mainMenu_cycleExact);
    fputs(buffer, f);
#endif
    snprintf((char*)buffer, 255, "customAutofireButton=%d\n",mainMenu_customAutofireButton);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "stylusOffset=%d\n",mainMenu_stylusOffset);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "tapDelay=%d\n",mainMenu_tapDelay);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "scanlines=%d\n",mainMenu_scanlines);
    fputs(buffer,f);
#if defined(PANDORA) || defined(ANDROIDSDL)
    snprintf((char*)buffer, 255, "ham=%d\n",1);
#else
    snprintf((char*)buffer, 255, "ham=%d\n",mainMenu_ham);
#endif
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "enableScreenshots=%d\n",mainMenu_enableScreenshots);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "floppyspeed=%d\n",mainMenu_floppyspeed);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "drives=%d\n",nr_drives);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "videomode=%d\n",mainMenu_ntsc);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mainMenu_cpuSpeed=%d\n",mainMenu_cpuSpeed);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "presetModeId=%d\n",presetModeId);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "moveX=%d\n",moveX);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "moveY=%d\n",moveY-16);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "displayedLines=%d\n",mainMenu_displayedLines);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "screenWidth=%d\n",screenWidth);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "cutLeft=%d\n",mainMenu_cutLeft);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "cutRight=%d\n",mainMenu_cutRight);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "customControls=%d\n",mainMenu_customControls);
    fputs(buffer,f);
#if !defined(__PSP2__) && !defined(__PS3__)
    snprintf((char*)buffer, 255, "custom_dpad=%d\n",mainMenu_custom_dpad[0]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_up=%d\n",mainMenu_custom_up[0]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_down=%d\n",mainMenu_custom_down[0]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_left=%d\n",mainMenu_custom_left[0]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_right=%d\n",mainMenu_custom_right[0]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_A=%d\n",mainMenu_custom_A[0]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_B=%d\n",mainMenu_custom_B[0]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_X=%d\n",mainMenu_custom_X[0]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_Y=%d\n",mainMenu_custom_Y[0]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_L=%d\n",mainMenu_custom_L[0]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_R=%d\n",mainMenu_custom_R[0]);
    fputs(buffer,f);
#else
	snprintf((char*)buffer, 255, "custom_controlSet=%d\n",mainMenu_custom_controlSet);
	fputs(buffer,f);
	for (int i=0; i<MAX_NUM_CONTROLLERS; i++)
	{
		for (int j=0; j<MAX_NUM_CUSTOM_PRESETS; j++)
		{
			snprintf((char*)buffer, 255, "custom%d_up_Ply%d=%d\n",j+1,i,mainMenu_customPreset_up[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_down_Ply%d=%d\n",j+1,i,mainMenu_customPreset_down[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_left_Ply%d=%d\n",j+1,i,mainMenu_customPreset_left[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_right_Ply%d=%d\n",j+1,i,mainMenu_customPreset_right[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_stickup_Ply%d=%d\n",j+1,i,mainMenu_customPreset_stickup[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_stickdown_Ply%d=%d\n",j+1,i,mainMenu_customPreset_stickdown[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_stickleft_Ply%d=%d\n",j+1,i,mainMenu_customPreset_stickleft[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_stickright_Ply%d=%d\n",j+1,i,mainMenu_customPreset_stickright[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_A_Ply%d=%d\n",j+1,i,mainMenu_customPreset_A[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_B_Ply%d=%d\n",j+1,i,mainMenu_customPreset_B[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_X_Ply%d=%d\n",j+1,i,mainMenu_customPreset_X[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_Y_Ply%d=%d\n",j+1,i,mainMenu_customPreset_Y[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_L_Ply%d=%d\n",j+1,i,mainMenu_customPreset_L[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_R_Ply%d=%d\n",j+1,i,mainMenu_customPreset_R[j][i]);
			fputs(buffer,f);
#ifdef __PS3__
			snprintf((char*)buffer, 255, "custom%d_L2_Ply%d=%d\n",j+1,i,mainMenu_customPreset_L2[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_R2_Ply%d=%d\n",j+1,i,mainMenu_customPreset_R2[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_L3_Ply%d=%d\n",j+1,i,mainMenu_customPreset_L3[j][i]);
			fputs(buffer,f);
			snprintf((char*)buffer, 255, "custom%d_R3_Ply%d=%d\n",j+1,i,mainMenu_customPreset_R3[j][i]);
			fputs(buffer,f);
#endif
		}
	}
#endif
    snprintf((char*)buffer, 255, "cpu=%d\n",mainMenu_CPU_model);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "chipset=%d\n",mainMenu_chipset);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "spritecollisions=%d\n",mainMenu_spriteCollisions);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "cpuspeed=%d\n",-mainMenu_CPU_speed);
    fputs(buffer,f);

    char namebuffer[256];
    if (uae4all_image_file0[0]) {
        strcpy(namebuffer,uae4all_image_file0);
        replace (namebuffer,'|',' ');
        snprintf((char*)buffer, 255, "df0=%s\n",namebuffer);
        fputs(buffer,f);
    } else {
        fputs("df0=\n",f);
    }
    if (uae4all_image_file1[0] && nr_drives > 1) {
        strcpy(namebuffer,uae4all_image_file1);
        replace (namebuffer,'|',' ');
        snprintf((char*)buffer, 255, "df1=%s\n",namebuffer);
        fputs(buffer,f);
    } else {
        fputs("df1=\n",f);
    }
    if (uae4all_image_file2[0] && nr_drives > 2) {
        strcpy(namebuffer,uae4all_image_file2);
        replace (namebuffer,'|',' ');
        snprintf((char*)buffer, 255, "df2=%s\n",namebuffer);
        fputs(buffer,f);
    } else {
            fputs("df2=\n",f);
    }
    if (uae4all_image_file3[0] && nr_drives > 3) {
        strcpy(namebuffer,uae4all_image_file3);
        replace (namebuffer,'|',' ');
        snprintf((char*)buffer, 255, "df3=%s\n",namebuffer);
        fputs(buffer,f);
    } else {
            fputs("df3=\n",f);
    }
#if defined(__PSP2__) || defined(__PS3__)
    snprintf((char*)buffer, 255, "cdimage=%s\n", current_cd_image[0] ? current_cd_image : "");
    fputs(buffer,f);
#endif
    snprintf((char*)buffer, 255, "script=%d\n",mainMenu_enableScripts);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "screenshot=%d\n",mainMenu_enableScreenshots);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "skipintro=%d\n",skipintro);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "boot_hd=%d\n",mainMenu_bootHD);
    fputs(buffer,f);
    if (uae4all_hard_dir[0] == '\0')
        snprintf((char*)buffer, 255, "hard_disk_dir=%s\n","*");
    else
        snprintf((char*)buffer, 255, "hard_disk_dir=%s\n",uae4all_hard_dir);
    fputs(buffer,f);
    if (uae4all_hard_file0[0] == '\0')
        snprintf((char*)buffer, 255, "hard_disk_file0=%s\n","*");
    else
        snprintf((char*)buffer, 255, "hard_disk_file0=%s\n",uae4all_hard_file0);
    fputs(buffer,f);
    if (uae4all_hard_file1[0] == '\0')
        snprintf((char*)buffer, 255, "hard_disk_file1=%s\n","*");
    else
        snprintf((char*)buffer, 255, "hard_disk_file1=%s\n",uae4all_hard_file1);
    fputs(buffer,f);
    if (uae4all_hard_file2[0] == '\0')
        snprintf((char*)buffer, 255, "hard_disk_file2=%s\n","*");
    else
        snprintf((char*)buffer, 255, "hard_disk_file2=%s\n",uae4all_hard_file2);
    fputs(buffer,f);
    if (uae4all_hard_file3[0] == '\0')
        snprintf((char*)buffer, 255, "hard_disk_file3=%s\n","*");
    else
        snprintf((char*)buffer, 255, "hard_disk_file3=%s\n",uae4all_hard_file3);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "hard_disk_file_ro=%d,%d,%d,%d\n",
             uae4all_hard_file_ro[0], uae4all_hard_file_ro[1],
             uae4all_hard_file_ro[2], uae4all_hard_file_ro[3]);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "chipmemory=%d\n",mainMenu_chipMemory);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "slowmemory=%d\n",mainMenu_slowMemory);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "fastmemory=%d\n",mainMenu_fastMemory);
    fputs(buffer,f);
#ifdef ANDROIDSDL
    snprintf((char*)buffer, 255, "onscreen=%d\n",mainMenu_onScreen);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "onScreen_textinput=%d\n",mainMenu_onScreen_textinput);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "onScreen_dpad=%d\n",mainMenu_onScreen_dpad);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "onScreen_button1=%d\n",mainMenu_onScreen_button1);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "onScreen_button2=%d\n",mainMenu_onScreen_button2);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "onScreen_button3=%d\n",mainMenu_onScreen_button3);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "onScreen_button4=%d\n",mainMenu_onScreen_button4);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "onScreen_button5=%d\n",mainMenu_onScreen_button5);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "onScreen_button6=%d\n",mainMenu_onScreen_button6);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "custom_position=%d\n",mainMenu_custom_position);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_x_textinput=%d\n",mainMenu_pos_x_textinput);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_y_textinput=%d\n",mainMenu_pos_y_textinput);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_x_dpad=%d\n",mainMenu_pos_x_dpad);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_y_dpad=%d\n",mainMenu_pos_y_dpad);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_x_button1=%d\n",mainMenu_pos_x_button1);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_y_button1=%d\n",mainMenu_pos_y_button1);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_x_button2=%d\n",mainMenu_pos_x_button2);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_y_button2=%d\n",mainMenu_pos_y_button2);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_x_button3=%d\n",mainMenu_pos_x_button3);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_y_button3=%d\n",mainMenu_pos_y_button3);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_x_button4=%d\n",mainMenu_pos_x_button4);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_y_button4=%d\n",mainMenu_pos_y_button4);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_x_button5=%d\n",mainMenu_pos_x_button5);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_y_button5=%d\n",mainMenu_pos_y_button5);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_x_button6=%d\n",mainMenu_pos_x_button6);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pos_y_button6=%d\n",mainMenu_pos_y_button6);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "quick_switch=%d\n",mainMenu_quickSwitch);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "FloatingJoystick=%d\n",mainMenu_FloatingJoystick);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "button_size=%f\n",mainMenu_button_size);
    fputs(buffer,f);
#endif
#if defined(ANDROIDSDL) || defined(AROS)
	snprintf((char*)buffer, 255, "VSync=%d\n",mainMenu_vsync);
 	fputs(buffer,f);
#endif

    strcpy(namebuffer,custom_kickrom);
    replace (namebuffer,'|',' ');
    snprintf((char*)buffer, 255, "custom_kickrom=%s\n",namebuffer);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "useSavesFolder=%d\n",mainMenu_useSavesFolder);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "footerSize=%d\n",mainMenu_footerSize);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "screenOffsetY=%d\n",mainMenu_screenOffsetY);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "screenOffsetX=%d\n",mainMenu_screenOffsetX);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "autoCrop=%d\n",mainMenu_autoCrop);
    fputs(buffer,f);
#ifdef __PS3__
    snprintf((char*)buffer, 255, "swapAB=%d\n",mainMenu_swapAB);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "singleJoycons=%d\n",mainMenu_singleJoycons);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mouseDevice=%d\n",mainMenu_mouseDevice);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mouseAcceleration=%d\n",mainMenu_mouseAcceleration);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mouseSlowFactor=%d\n",mainMenu_mouseSlowFactor);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mouseSlowButton=%d\n",mainMenu_mouseSlowButton);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mouseFastFactor=%d\n",mainMenu_mouseFastFactor);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mouseFastButton=%d\n",mainMenu_mouseFastButton);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "mouseSwapButtons=%d\n",mainMenu_mouseSwapButtons);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pinballMode=%d\n", mainMenu_pinballMode);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "rtgMemory=%d\n", mainMenu_rtgMemory);
    fputs(buffer,f);
#endif
#ifdef __PSP2__
    snprintf((char*)buffer, 255, "cdimage=%s\n", current_cd_image);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "diskSoundVolume=%d\n", mainMenu_diskSoundVolume);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "diskSoundVolumeConfigured=1\n");
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "autoEjectFloppy=%d\n", mainMenu_autoEjectFloppy);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "midiSynth=%d\n", mainMenu_midiSynth);
    fputs(buffer,f);
    snprintf((char*)buffer, 255, "pinballMode=%d\n", mainMenu_pinballMode);
    fputs(buffer,f);
#endif
    fclose(f);
    return 1;
}


void loadconfig(int general)
{

    char path[300];

    if(general == 1) {
        snprintf(path, 300, "%s/conf/adfdir.conf", launchDir);
        FILE *f1=fopen(path,"rt");
        if(!f1) {
            printf ("No config file %s!\n",path);
            strcpy(currentDir, launchDir);
#ifdef ANDROIDSDL
            char *dirname="../../com.cloanto.amigaforever.essentials/files/adf/";
            struct stat statbuf;
            stat (dirname, &statbuf);
            if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
                strcat(currentDir, "/../../com.cloanto.amigaforever.essentials/files/adf");
            else
#endif
                strcat(currentDir, "/roms/");
        } else {
            fscanf(f1,"path=%299s\n", currentDir);
            fclose(f1);
            printf("adfdir loaded. currentDir=%s\n", currentDir);
        }
    }

    if (general == 1)
        if (config_load_filename[0] != 0) {
            snprintf(path, 300, "%s",config_load_filename);
        }
        else {
            snprintf(path, 300, "%s/conf/uaeconfig.conf", launchDir);
        }
    else if (general == 3)
        snprintf(path, 300, config_filename, launchDir);
    else if(general == 0)
        create_configfilename(path, uae4all_image_file0, 0);
    else if (general == 2) {
        path[0] = '\0';
        if(uae4all_hard_file0[0] != '\0')
            create_configfilename(path, uae4all_hard_file0, 0);
        if(path[0] == '\0')
            return;
    }
    else if (general == 4) {
        path[0] = '\0';
        if(uae4all_hard_dir[0] != '\0')
            create_configfilename(path, uae4all_hard_dir, 1);
        if(path[0] == '\0')
            return;
    }
    else if (general == 5) {
        snprintf(path, 300, config_load_filename);
    }

    FILE *f=fopen(path,"rt");
    if (!f) {
        printf ("No config file %s!\n",path);
    } else {
        SetDefaultMenuSettings(general);

        char filebuffer[256];
#if defined(PANDORA) || defined(ANDROIDSDL)
        int dummy;
#endif
        fscanf(f,"kickstart=%d\n",&kickstart);
#if defined(PANDORA) || defined(ANDROIDSDL)
        fscanf(f,"scaling=%d\n",&dummy);
#else
        fscanf(f,"scaling=%d\n",&mainMenu_enableHWscaling);
#endif
#if defined(__PSP2__) || defined(__PS3__)
        fscanf(f,"shader=%d\n",&mainMenu_shader);
#if defined(__PS3__)
        if (mainMenu_shader > 9) mainMenu_shader = 1;
        if (mainMenu_shader == 6) mainMenu_shader = 0;
#endif
        fscanf(f,"leftstickmouse=%d\n",&mainMenu_leftStickMouse);
        fscanf(f,"touchcontrols=%d\n",&mainMenu_touchControls);
        fscanf(f,"deadzone=%d\n",&mainMenu_deadZone);
#endif
        fscanf(f,"showstatus=%d\n",&mainMenu_showStatus);
        fscanf(f,"background=%d\n",&mainMenu_background);
        fscanf(f,"font=%d\n",&mainMenu_font);
        fscanf(f,"mousemultiplier=%d\n",&mainMenu_mouseMultiplier);
        if (mainMenu_mouseMultiplier < 10) {
            if (mainMenu_mouseMultiplier == 1 || mainMenu_mouseMultiplier == 2 || mainMenu_mouseMultiplier == 4) {
                mainMenu_mouseMultiplier *= 100;
            }
            else {
                mainMenu_mouseMultiplier = DEFAULT_MOUSEMULTIPLIER;
            }
        }
        fscanf(f,"mouseemulation=%d\n",&mainMenu_mouseEmulation );
#if defined(PANDORA) || defined(ANDROIDSDL)
        fscanf(f,"systemclock=%d\n",&dummy);
        fscanf(f,"syncthreshold=%d\n", &dummy);
#else
        fscanf(f,"systemclock=%d\n",&mainMenu_throttle);
        fscanf(f,"syncthreshold=%d\n", &timeslice_mode);
#endif
        fscanf(f,"frameskip=%d\n",&mainMenu_frameskip);
        fscanf(f,"vkbdlanguage=%d\n",&mainMenu_vkbdLanguage);
        fscanf(f,"vkbdstyle=%d\n",&mainMenu_vkbdStyle);
        fscanf(f,"vkbdtransparency=%d\n",&mainMenu_vkbdTransparency);
        fscanf(f,"vkbdposition=%d\n",&mainMenu_vkbdPosition);
        fscanf(f,"sound=%d\n",&mainMenu_sound );
        if (mainMenu_sound >= 10) {
            mainMenu_soundStereo = 1;
            mainMenu_sound -= 10;
        } else
            mainMenu_soundStereo = 0;
        if (fscanf(f,"soundstereosep=%d\n",&mainMenu_soundStereoSep) != 1)
            mainMenu_soundStereoSep = 4;
        if (mainMenu_soundStereoSep < 0 || mainMenu_soundStereoSep > 4)
            mainMenu_soundStereoSep = 4;
        fscanf(f,"soundrate=%d\n",&sound_rate);
#ifdef __PS3__
        sound_rate = DEFAULT_SOUND_FREQ;
#endif
        fscanf(f,"autosave=%d\n",&mainMenu_autosave);
        fscanf(f,"gp2xclock=%d\n", &gp2xClockSpeed);
        int joybuffer = 0;
        fscanf(f,"joyconf=%d\n",&joybuffer);
        mainMenu_joyConf = (joybuffer & 0x0f);
        mainMenu_joyPort = ((joybuffer >> 4) & 0x0f);
        if (mainMenu_joyPort == 0)
            mainMenu_joyPort = 2;
        fscanf(f,"autofireRate=%d\n",&mainMenu_autofireRate);
        fscanf(f,"autofire=%d\n",&mainMenu_autofire);
        fscanf(f,"autofireMode=%d\n",&mainMenu_autofireMode);
#if defined(__PSP2__) || defined(__PS3__)
        fscanf(f,"whdload_args=%255[^\n]\n", mainMenu_whdload_args);
        fscanf(f,"whdload_mode=%d\n", &mainMenu_whdload_mode);
        fscanf(f,"whdload_blitter=%d\n", &mainMenu_whdload_blitter);
        for (int i = 0; i < 4; i++)
            fscanf(f,"floppyWriteProtect%d=%d\n", i, &mainMenu_floppyWriteProtect[i]);
        fscanf(f,"cycleExact=%d\n", &mainMenu_cycleExact);
#endif
        fscanf(f,"customAutofireButton=%d\n",&mainMenu_customAutofireButton);
        mainMenu_customAutofireButton = 0;
        fscanf(f,"stylusOffset=%d\n",&mainMenu_stylusOffset);
        fscanf(f,"tapDelay=%d\n",&mainMenu_tapDelay);
        fscanf(f,"scanlines=%d\n",&mainMenu_scanlines);
#if defined(PANDORA) || defined(ANDROIDSDL)
        fscanf(f,"ham=%d\n",&dummy);
#else
        fscanf(f,"ham=%d\n",&mainMenu_ham);
#endif
        fscanf(f,"enableScreenshots=%d\n",&mainMenu_enableScreenshots);
        fscanf(f,"floppyspeed=%d\n",&mainMenu_floppyspeed);
        fscanf(f,"drives=%d\n",&nr_drives);
        fscanf(f,"videomode=%d\n",&mainMenu_ntsc);
        fscanf(f,"mainMenu_cpuSpeed=%d\n",&mainMenu_cpuSpeed);
        fscanf(f,"presetModeId=%d\n",&presetModeId);
        fscanf(f,"moveX=%d\n",&moveX);
        fscanf(f,"moveY=%d\n",&moveY);
        moveY+=16;
        fscanf(f,"displayedLines=%d\n",&mainMenu_displayedLines);
        fscanf(f,"screenWidth=%d\n",&screenWidth);
        fscanf(f,"cutLeft=%d\n",&mainMenu_cutLeft);
        fscanf(f,"cutRight=%d\n",&mainMenu_cutRight);
        fscanf(f,"customControls=%d\n",&mainMenu_customControls);
#if !defined(__PSP2__) && !defined(__PS3__)
        fscanf(f,"custom_dpad=%d\n",&mainMenu_custom_dpad);
        fscanf(f,"custom_up=%d\n",&mainMenu_custom_up[0]);
        fscanf(f,"custom_down=%d\n",&mainMenu_custom_down[0]);
        fscanf(f,"custom_left=%d\n",&mainMenu_custom_left[0]);
        fscanf(f,"custom_right=%d\n",&mainMenu_custom_right[0]);
        fscanf(f,"custom_A=%d\n",&mainMenu_custom_A[0]);
        fscanf(f,"custom_B=%d\n",&mainMenu_custom_B[0]);
        fscanf(f,"custom_X=%d\n",&mainMenu_custom_X[0]);
        fscanf(f,"custom_Y=%d\n",&mainMenu_custom_Y[0]);
        fscanf(f,"custom_L=%d\n",&mainMenu_custom_L[0]);
        fscanf(f,"custom_R=%d\n",&mainMenu_custom_R[0]);
#else
		fscanf(f,"custom_controlSet=%d\n",&mainMenu_custom_controlSet);
		int config_1_73 = -1;
		int config_1_82 = -1;
		int config_1_83 = -1;
		int l=0;
		int m=0;
		for (int i=0; i<MAX_NUM_CONTROLLERS; i++)
		{
			if (config_1_82==1)
				break;
			for (int j=0; j<MAX_NUM_CUSTOM_PRESETS; j++) {
#ifdef __PS3__
				if (!(i==0 && j==1)) {
#endif
				if (config_1_83 == -1) {
					if (!fscanf(f,"custom%d\n",&l)) {
						config_1_82 = 1;
						break;
					}
					if (j==3 && l!=4) {
						config_1_83 = 1;
						break;
					}
				}
				else
					config_1_83 = -1;
				fscanf(f,"_up_Ply%d=%d\n",&m,&mainMenu_customPreset_up[j][i]);
#ifdef __PS3__
				}
#endif
				fscanf(f,"custom%d_down_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_down[j][i]);
				fscanf(f,"custom%d_left_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_left[j][i]);
				fscanf(f,"custom%d_right_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_right[j][i]);
				if (fscanf(f,"custom%d_stickup_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_stickup[j][i]) > 1) {
					fscanf(f,"custom%d_stickdown_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_stickdown[j][i]);
					fscanf(f,"custom%d_stickleft_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_stickleft[j][i]);
					fscanf(f,"custom%d_stickright_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_stickright[j][i]);
					fscanf(f,"custom%d_A_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_A[j][i]);
				} else {
					mainMenu_customPreset_stickup[j][i] = mainMenu_customPreset_up[j][i];
					mainMenu_customPreset_stickdown[j][i] = mainMenu_customPreset_down[j][i];
					mainMenu_customPreset_stickleft[j][i] = mainMenu_customPreset_left[j][i];
					mainMenu_customPreset_stickright[j][i] = mainMenu_customPreset_right[j][i];
					fscanf(f,"A_Ply%d=%d\n",&m,&mainMenu_customPreset_A[j][i]);
				}
				fscanf(f,"custom%d_B_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_B[j][i]);
				fscanf(f,"custom%d_X_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_X[j][i]);
				fscanf(f,"custom%d_Y_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_Y[j][i]);
				fscanf(f,"custom%d_L_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_L[j][i]);
				fscanf(f,"custom%d_R_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_R[j][i]);
#ifdef __PS3__
				if (j == 0 && i == 0) {
					if (fscanf(f,"custom%d_L2_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_L2[j][i]) > 1) {
						fscanf(f,"custom%d_R2_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_R2[j][i]);
						fscanf(f,"custom%d_L3_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_L3[j][i]);
						fscanf(f,"custom%d_R3_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_R3[j][i]);
						fscanf(f,"custom%d_up_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_up[j+1][i]);
						config_1_73 = 0;
					} else {
						fscanf(f,"up_Ply%d=%d\n",&m,&mainMenu_customPreset_up[j+1][i]);
						config_1_73 = 1;
					}
				} else {
					if (config_1_73 == 0) {
						fscanf(f,"custom%d_L2_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_L2[j][i]);
						fscanf(f,"custom%d_R2_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_R2[j][i]);
						fscanf(f,"custom%d_L3_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_L3[j][i]);
						fscanf(f,"custom%d_R3_Ply%d=%d\n",&l,&m,&mainMenu_customPreset_R3[j][i]);
					}
				}
#endif
			}
		}
		remap_custom_controls();
		if (config_1_82 == 1) {
			fscanf(f,"pu=%d\n",&mainMenu_CPU_model);
		} else
#endif
		fscanf(f,"cpu=%d\n",&mainMenu_CPU_model);
        fscanf(f,"chipset=%d\n",&mainMenu_chipset);
        fscanf(f,"spritecollisions=%d\n",&mainMenu_spriteCollisions);
        fscanf(f,"cpuspeed=%d\n",&mainMenu_CPU_speed);
        if(mainMenu_CPU_speed < 0) {
            mainMenu_CPU_speed = -mainMenu_CPU_speed;
        }

        memset(filebuffer, 0, 256);
        if (fscanf(f,"df0%s\n",filebuffer)) {
            replace(filebuffer,' ','|');
            strcpy(uae4all_image_file0,filebuffer+1);
            extractFileName(uae4all_image_file0,filename0);
        } else {
            memset(uae4all_image_file0, 0, 256);
            memset(filename0, 0, 256);
        }
        if (fscanf(f,"df1%s\n",filebuffer)) {
            replace(filebuffer,' ','|');
            strcpy(uae4all_image_file1,filebuffer+1);
            extractFileName(uae4all_image_file1,filename1);
        } else {
            memset(uae4all_image_file1, 0, 256);
            memset(filename1, 0, 256);
        }
        if (fscanf(f,"df2%s\n",filebuffer)) {
            replace(filebuffer,' ','|');
            strcpy(uae4all_image_file2,filebuffer+1);
            extractFileName(uae4all_image_file2,filename2);
        } else {
            memset(uae4all_image_file2, 0, 256);
            memset(filename2, 0, 256);
        }
        if (fscanf(f,"df3%s\n",filebuffer)) {
            replace(filebuffer,' ','|');
            strcpy(uae4all_image_file3,filebuffer+1);
            extractFileName(uae4all_image_file3,filename3);
        } else {
            memset(uae4all_image_file3, 0, 256);
            memset(filename3, 0, 256);
        }

        mainMenu_drives=nr_drives;
        int oldconfig = fscanf(f,"script=%d\n",&mainMenu_enableScripts);
        if (oldconfig != 0) {
            fscanf(f,"screenshot=%d\n", &mainMenu_enableScreenshots);
            fscanf(f,"skipintro=%d\n", &skipintro);
            fscanf(f,"boot_hd=%d\n",&mainMenu_bootHD);

            fscanf(f,"hard_disk_dir=",uae4all_hard_dir);
            uae4all_hard_dir[0] = '\0';
            {
                char c[2] = {0, 0};
                *c = fgetc(f);
                while (*c && (*c != '\n')) {
                    strcat(uae4all_hard_dir, c);
                    *c = fgetc(f);
                }
            }
            if (uae4all_hard_dir[0] == '*')
                uae4all_hard_dir[0] = '\0';
            fscanf(f,"hard_disk_file0=",uae4all_hard_file0);
            uae4all_hard_file0[0] = '\0';
            {
                char c[2] = {0, 0};
                *c = fgetc(f);
                while (*c && (*c != '\n')) {
                    strcat(uae4all_hard_file0, c);
                    *c = fgetc(f);
                }
            }
            if (uae4all_hard_file0[0] == '*')
                uae4all_hard_file0[0] = '\0';
            fscanf(f,"hard_disk_file1=",uae4all_hard_file1);
            uae4all_hard_file1[0] = '\0';
            {
                char c[2] = {0, 0};
                *c = fgetc(f);
                while (*c && (*c != '\n')) {
                    strcat(uae4all_hard_file1, c);
                    *c = fgetc(f);
                }
            }
            if (uae4all_hard_file1[0] == '*')
                uae4all_hard_file1[0] = '\0';
            fscanf(f,"hard_disk_file2=",uae4all_hard_file2);
            uae4all_hard_file2[0] = '\0';
            {
                char c[2] = {0, 0};
                *c = fgetc(f);
                while (*c && (*c != '\n')) {
                    strcat(uae4all_hard_file2, c);
                    *c = fgetc(f);
                }
            }
            if (uae4all_hard_file2[0] == '*')
                uae4all_hard_file2[0] = '\0';
            fscanf(f,"hard_disk_file3=",uae4all_hard_file3);
            uae4all_hard_file3[0] = '\0';
            {
                char c[2] = {0, 0};
                *c = fgetc(f);
                while (*c && (*c != '\n')) {
                    strcat(uae4all_hard_file3, c);
                    *c = fgetc(f);
                }
            }
            if (uae4all_hard_file3[0] == '*')
                uae4all_hard_file3[0] = '\0';
        }
        fscanf(f,"hard_disk_file_ro=%d,%d,%d,%d\n",
               &uae4all_hard_file_ro[0], &uae4all_hard_file_ro[1],
               &uae4all_hard_file_ro[2], &uae4all_hard_file_ro[3]);
        fscanf(f,"chipmemory=%d\n",&mainMenu_chipMemory);
        fscanf(f,"slowmemory=%d\n",&mainMenu_slowMemory);
        fscanf(f,"fastmemory=%d\n",&mainMenu_fastMemory);
#ifdef ANDROIDSDL
        fscanf(f,"onscreen=%d\n",&mainMenu_onScreen);
        fscanf(f,"onScreen_textinput=%d\n",&mainMenu_onScreen_textinput);
        fscanf(f,"onScreen_dpad=%d\n",&mainMenu_onScreen_dpad);
        fscanf(f,"onScreen_button1=%d\n",&mainMenu_onScreen_button1);
        fscanf(f,"onScreen_button2=%d\n",&mainMenu_onScreen_button2);
        fscanf(f,"onScreen_button3=%d\n",&mainMenu_onScreen_button3);
        fscanf(f,"onScreen_button4=%d\n",&mainMenu_onScreen_button4);
        fscanf(f,"onScreen_button5=%d\n",&mainMenu_onScreen_button5);
        fscanf(f,"onScreen_button6=%d\n",&mainMenu_onScreen_button6);
        fscanf(f,"custom_position=%d\n",&mainMenu_custom_position);
        fscanf(f,"pos_x_textinput=%d\n",&mainMenu_pos_x_textinput);
        fscanf(f,"pos_y_textinput=%d\n",&mainMenu_pos_y_textinput);
        fscanf(f,"pos_x_dpad=%d\n",&mainMenu_pos_x_dpad);
        fscanf(f,"pos_y_dpad=%d\n",&mainMenu_pos_y_dpad);
        fscanf(f,"pos_x_button1=%d\n",&mainMenu_pos_x_button1);
        fscanf(f,"pos_y_button1=%d\n",&mainMenu_pos_y_button1);
        fscanf(f,"pos_x_button2=%d\n",&mainMenu_pos_x_button2);
        fscanf(f,"pos_y_button2=%d\n",&mainMenu_pos_y_button2);
        fscanf(f,"pos_x_button3=%d\n",&mainMenu_pos_x_button3);
        fscanf(f,"pos_y_button3=%d\n",&mainMenu_pos_y_button3);
        fscanf(f,"pos_x_button4=%d\n",&mainMenu_pos_x_button4);
        fscanf(f,"pos_y_button4=%d\n",&mainMenu_pos_y_button4);
        fscanf(f,"pos_x_button5=%d\n",&mainMenu_pos_x_button5);
        fscanf(f,"pos_y_button5=%d\n",&mainMenu_pos_y_button5);
        fscanf(f,"pos_x_button6=%d\n",&mainMenu_pos_x_button6);
        fscanf(f,"pos_y_button6=%d\n",&mainMenu_pos_y_button6);
        fscanf(f,"quick_switch=%d\n",&mainMenu_quickSwitch);
        fscanf(f,"FloatingJoystick=%d\n",&mainMenu_FloatingJoystick);
        fscanf(f,"button_size=%f\n",&mainMenu_button_size);
#endif
#if defined(ANDROIDSDL) || defined(AROS)
		fscanf(f,"VSync=%d\n",&mainMenu_vsync);
#endif
        memset(filebuffer, 0, 256);
        fscanf(f,"custom_kickrom=%s\n",filebuffer);
        replace(filebuffer,' ','|');
        if (filebuffer[0]) {
            strcpy(custom_kickrom, filebuffer);
        }
        fscanf(f,"useSavesFolder=%d\n",&mainMenu_useSavesFolder);
        mainMenu_useSavesFolder = DEFAULT_USESAVESFOLDER;
        if (fscanf(f,"footerSize=%d\n",&mainMenu_footerSize) != 1)
            mainMenu_footerSize = 0;
        if (mainMenu_footerSize < -64) mainMenu_footerSize = -64;
        if (mainMenu_footerSize > 160) mainMenu_footerSize = 160;
        if (fscanf(f,"screenOffsetY=%d\n",&mainMenu_screenOffsetY) != 1)
            mainMenu_screenOffsetY = 0;
        if (mainMenu_screenOffsetY < -256) mainMenu_screenOffsetY = -256;
        if (mainMenu_screenOffsetY > 256) mainMenu_screenOffsetY = 256;
        if (fscanf(f,"screenOffsetX=%d\n",&mainMenu_screenOffsetX) != 1)
            mainMenu_screenOffsetX = 0;
        if (mainMenu_screenOffsetX < -256) mainMenu_screenOffsetX = -256;
        if (mainMenu_screenOffsetX > 256) mainMenu_screenOffsetX = 256;
        if (fscanf(f,"autoCrop=%d\n",&mainMenu_autoCrop) != 1)
            mainMenu_autoCrop = 0;
        if (mainMenu_autoCrop < 0 || mainMenu_autoCrop > 3)
            mainMenu_autoCrop = 0;
#ifdef __PS3__
        fscanf(f,"swapAB=%d\n",&mainMenu_swapAB);
        fscanf(f,"singleJoycons=%d\n",&mainMenu_singleJoycons);
        if (fscanf(f,"mouseDevice=%d\n",&mainMenu_mouseDevice) != 1) mainMenu_mouseDevice = 0;
        if (fscanf(f,"mouseAcceleration=%d\n",&mainMenu_mouseAcceleration) != 1) mainMenu_mouseAcceleration = 0;
        if (fscanf(f,"mouseSlowFactor=%d\n",&mainMenu_mouseSlowFactor) != 1) mainMenu_mouseSlowFactor = 2;
        if (fscanf(f,"mouseSlowButton=%d\n",&mainMenu_mouseSlowButton) != 1) mainMenu_mouseSlowButton = 1;
        if (fscanf(f,"mouseFastFactor=%d\n",&mainMenu_mouseFastFactor) != 1) mainMenu_mouseFastFactor = 2;
        if (fscanf(f,"mouseFastButton=%d\n",&mainMenu_mouseFastButton) != 1) mainMenu_mouseFastButton = 2;
        if (fscanf(f,"mouseSwapButtons=%d\n",&mainMenu_mouseSwapButtons) != 1) mainMenu_mouseSwapButtons = 0;
        if (fscanf(f,"pinballMode=%d\n",&mainMenu_pinballMode) != 1) mainMenu_pinballMode = 0;
        if (fscanf(f,"rtgMemory=%d\n",&mainMenu_rtgMemory) != 1) mainMenu_rtgMemory = 0;
        if (mainMenu_rtgMemory < 0 || mainMenu_rtgMemory > 2) mainMenu_rtgMemory = 0;
#endif
#if defined(__PSP2__) || defined(__PS3__)
        memset(filebuffer, 0, 256);
        if (fscanf(f,"cdimage=%255[^\n]\n",filebuffer) == 1 && filebuffer[0] != '\0') {
            if (cdrom_open_image(filebuffer))
                ApplyCd32Profile();
        } else
            cdrom_close_image();
        int disk_sound_volume_configured = 0;
        if (fscanf(f,"diskSoundVolume=%d\n", &mainMenu_diskSoundVolume) != 1)
            mainMenu_diskSoundVolume = 35;
        fscanf(f,"diskSoundVolumeConfigured=%d\n", &disk_sound_volume_configured);
        if (!disk_sound_volume_configured && mainMenu_diskSoundVolume == 100)
            mainMenu_diskSoundVolume = 35;
        if (mainMenu_diskSoundVolume < 0) mainMenu_diskSoundVolume = 0;
        if (mainMenu_diskSoundVolume > 100) mainMenu_diskSoundVolume = 100;
        disk_sound_set_volume(mainMenu_diskSoundVolume);
        if (fscanf(f,"autoEjectFloppy=%d\n", &mainMenu_autoEjectFloppy) != 1)
            mainMenu_autoEjectFloppy = 1;
        if (fscanf(f,"midiSynth=%d\n", &mainMenu_midiSynth) != 1)
            mainMenu_midiSynth = 0;
        midi_synth_set_enabled(mainMenu_midiSynth);
        if (fscanf(f,"pinballMode=%d\n", &mainMenu_pinballMode) != 1)
            mainMenu_pinballMode = 0;
#endif
        fclose(f);
    }
    int old_displayedLines = mainMenu_displayedLines;
    SetPresetMode(presetModeId);
    mainMenu_displayedLines = old_displayedLines;
    UpdateCPUModelSettings();
    UpdateChipsetSettings();
#ifdef USE_GUICHAN
    if (running==false)
#endif
    {
#if !defined(__PSP2__) && !defined(__PS3__)
        update_display();
#endif
    }
    UpdateMemorySettings();
    set_joyConf();
}
