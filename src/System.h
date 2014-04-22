#ifndef SYSTEM_H
#define SYSTEM_H

#include <string.h>
#include "common/Types.h"

#include <zlib.h>

#include <stdio.h>
extern int   g_logtofile;
extern FILE *g_flogfile;

#if __DEUBG__

#include <sys/slog.h>
#include <sys/slogcodes.h>
#include "bbDialog.h"


#undef SLOG
#define SLOG(fmt, ...) \
	if (g_logtofile)    \
	{    fprintf(g_flogfile, "[GBA-LOG][%s:%d]:"fmt"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); fflush(g_flogfile); } \
    else               \
        /*slogf(_SLOG_SETCODE(_SLOGC_TEST+328, 0), _SLOG_DEBUG1, "[GBA-LOG][%s:%d]:"fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)*/ \
	    fprintf(stdout, "[GBA-LOG][%s:%d]:"fmt"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); fflush(stdout)
#else
#define SLOG(fmt, ...)      \
		if (g_logtofile)    \
		{                   \
			fprintf(g_flogfile, "[GBA-LOG][%s:%d]:"fmt"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); fflush(g_flogfile); \
		}

#define ASSERT( exp )       \
		if ( !(exp) )       \
		{                   \
			SLOG("[ASSERTION] (%s)", #exp); \
		}
#endif

#define systemMessage(num, fmt, ...) SLOG(#num"|"fmt, ##__VA_ARGS__)

class SoundDriver;

struct EmulatedSystem {
  // main emulation function
  void (*emuMain)(int);
  // reset emulator
  void (*emuReset)();
  // clean up memory
  void (*emuCleanUp)();
  // load battery file
  bool (*emuReadBattery)(const char *);
  // write battery file
  bool (*emuWriteBattery)(const char *);
  // load state
  bool (*emuReadState)(const char *);
  // save state
  bool (*emuWriteState)(const char *);
  // load memory state (rewind)
  bool (*emuReadMemState)(char *, int);
  // write memory state (rewind)
  bool (*emuWriteMemState)(char *, int);
  // write PNG file
  bool (*emuWritePNG)(const char *);
  // write BMP file
  bool (*emuWriteBMP)(const char *);
  // emulator update CPSR (ARM only)
  void (*emuUpdateCPSR)();
  // emulator has debugger
  bool emuHasDebugger;
  // clock ticks to emulate
  int emuCount;
};

extern void log(const char *,...);

extern bool systemPauseOnFrame();
extern void systemGbPrint(u8 *,int,int,int,int,int);
extern void systemScreenCapture(int);
extern void systemDrawScreen();
// updates the joystick data
extern bool systemReadJoypads();
// return information about the given joystick, -1 for default joystick
extern u32 systemReadJoypad(int);
extern u32 systemGetClock();
//extern void systemMessage(int, const char *, ...);
extern void systemSetTitle(const char *);
extern SoundDriver * systemSoundInit();
extern void systemOnWriteDataToSoundBuffer(const u16 * finalWave, int length);
extern void systemOnSoundShutdown();
extern void systemScreenMessage(const char *);
extern void systemUpdateMotionSensor();
extern int  systemGetSensorX();
extern int  systemGetSensorY();
extern bool systemCanChangeSoundQuality();
extern void systemShowSpeed(int);
extern void system10Frames(int);
extern void systemFrame();
extern void systemGbBorderOn();
extern void systemFrameInit(void);

extern void Sm60FPS_Init();
extern bool Sm60FPS_CanSkipFrame();
extern void Sm60FPS_Sleep();
extern void DbgMsg(const char *msg, ...);
#ifdef SDL
#define winlog log
#else
extern void winlog(const char *,...);
#endif

extern void (*dbgOutput)(const char *s, u32 addr);
extern void (*dbgSignal)(int sig,int number);

extern u16 systemColorMap16[0x10000];
extern u32 systemColorMap32[0x10000];
extern u16 systemGbPalette[24];
extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;
extern int systemColorDepth;
extern int systemDebug;
extern int systemVerbose;
extern int systemFrameSkip;
extern int systemRenderFps;
extern int systemSaveUpdateCounter;
extern int systemSpeed;

#define SYSTEM_SAVE_UPDATED 30
#define SYSTEM_SAVE_NOT_UPDATED 0
#define INLINE inline
#define LSB_FIRST

#ifdef  STL100_1
#define GBA_USE_RGBA8888 // comment out to use 16bit GFX
#endif

#endif // SYSTEM_H
