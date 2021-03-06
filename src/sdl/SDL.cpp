// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005-2006 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cmath>
#include <fstream>
#include <errno.h>

#ifdef __APPLE__
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>
#else
#ifndef NO_OGL
#ifdef __PLAYBOOK__
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <GL/glu.h>
#include <GL/glext.h>
#endif // __PLAYBOOK__
#endif // NO_OGL
#endif //__APPLE__

using namespace std;
#include <time.h>

#include "../AutoBuild.h"

#include <SDL.h>

#include "../common/Patch.h"
#include "../common/Port.h"
#include "../gba/GBA.h"
#include "../gba/GBAcpu.h"
#include "../gba/agbprint.h"
#include "../gba/Flash.h"
#include "../gba/Cheats.h"
#include "../gba/RTC.h"
#include "../gba/Sound.h"
#include "../gb/gb.h"
#include "../gb/gbGlobals.h"
#include "../gb/gbCheats.h"
#include "../gb/gbSound.h"
#include "../Util.h"
#include <bps/dialog.h>
#include <bps/bps.h>
#include <bps/event.h>
#include "../utils/xstring.h"

#ifndef __QNXNTO__
#include "debugger.h"
#endif

#include "filters.h"
#include "text.h"
#include "inputSDL.h"
#include "../common/SoundSDL.h"
#include "bbDialog.h"

#ifndef _WIN32
# include <unistd.h>
# define GETCWD getcwd
#else // _WIN32
# include <direct.h>
# include <stat.h>
# define GETCWD _getcwd
#endif // _WIN32

#ifndef __GNUC__
# define HAVE_DECL_GETOPT 0
# define __STDC__ 1
# include "getopt.h"
#else // ! __GNUC__
# define HAVE_DECL_GETOPT 1
# include <getopt.h>
#endif // ! __GNUC__

#if WITH_LIRC
#include <sys/poll.h>
#include <lirc/lirc_client.h>
#endif

bool InitLink(void);

#ifdef __PLAYBOOK__
static pthread_mutex_t loader_mutex = PTHREAD_MUTEX_INITIALIZER;
static vector<string> romList;
static vector<string> sortedRomList;

static vector<string> cheatList;
static vector<string> sortedCheatList;
#endif

extern void remoteInit();
extern void remoteCleanUp();
extern void remoteStubMain();
extern void remoteStubSignal(int,int);
extern void remoteOutput(const char *, u32);
extern void remoteSetProtocol(int);
extern void remoteSetPort(int);

void sdlInitVideo();

char g_runningFile_str[512];

static bbDialog *dbgDialog = NULL;

#ifdef NDEBUG
static char dbgString[256];

#define DLOG(fmt, ...) \
	if (dbgDialog)     \
	{                  \
		sprintf(dbgString, fmt, ##__VA_ARGS__); \
		dbgDialog->showNotification(dbgString); \
	}
#endif

struct EmulatedSystem emulator = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	false,
	0
};

SDL_Surface *g_surface = NULL;

int systemSpeed = 0;
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 0;
int systemDebug = 0;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemRenderFps = 60;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
int systemSoundQuality = 2;
int systemSoundDeclicking = 0;
int systemSoundRes = 0x30F;

int g_srcPitch = 0;
int g_srcWidth =  0;
int g_srcHeight = 0;
int g_destWidth = 0;
int g_destHeight = 0;
int g_destPitch  = 0;
int g_desktopWidth = 0;
int g_desktopHeight = 0;
int gameIndex = 0;
Filter filter = kStretch1x;


u8 *delta = NULL;

int filter_enlarge = 0;

int sdlPrintUsage = 0;

int cartridgeType = 3;
int captureFormat = 0;

int g_openGL      = 0;
int g_textureSize = 256;
int g_logtofile   = 0;
FILE *g_flogfile  = NULL;

u8 *filterPix = 0;

int pauseWhenInactive = 0;
int active = 1;
int emulating = 0;
int RGB_LOW_BITS_MASK=0x821;
u32 systemColorMap32[0x10000];
u16 systemColorMap16[0x10000];
u16 systemGbPalette[24];
FilterFunc filterFunction = 0;
IFBFilterFunc ifbFunction = 0;
IFBFilter ifbType = kIFBNone;
char filename[2048];
char biosFileName[2048];
char gbBiosFileName[2048];
char captureDir[2048];
char saveDir[2048];
char batteryDir[2048];
char* homeDir = NULL;

// Directory within homedir to use for default save location.
#define DOT_DIR "savegames"

static char *rewindMemory = NULL;
static int *rewindSerials = NULL;
static int rewindPos = 0;
static int rewindSerial = 0;
static int rewindTopPos = 0;
static int rewindCounter = 0;
static int rewindCount = 0;
static bool rewindSaveNeeded = false;
static int rewindTimer = 0;
static float curFps;

static int sdlSaveKeysSwitch = 0;
// if 0, then SHIFT+F# saves, F# loads (old VBA, ...)
// if 1, then SHIFT+F# loads, F# saves (linux snes9x, ...)
// if 2, then F5 decreases slot number, F6 increases, F7 saves, F8 loads

static int saveSlotPosition = 0; // default is the slot from normal F1
// internal slot number for undoing the last load
#define SLOT_POS_LOAD_BACKUP 8
// internal slot number for undoing the last save
#define SLOT_POS_SAVE_BACKUP 9

static int sdlOpenglScale = 0;
// will scale window on init by this much
static int sdlSoundToggledOff = 0;
// allow up to 100 IPS/UPS/PPF patches given on commandline
#define PATCH_MAX_NUM 100
int	sdl_patch_num	= 0;
char *	(sdl_patch_names[PATCH_MAX_NUM])	= { NULL }; // and so on

extern int autoFireMaxCount;

#define REWIND_NUM 8
#define REWIND_SIZE 400000
#define SYSMSG_BUFFER_SIZE 1024

#define _stricmp strcasecmp

bool wasPaused = false;
int autoFrameSkip = 1;
int frameskipadjust = 1;
int showRenderedFrames = 0;
int renderedFrames = 0;

u32 throttleLastTime = 0;
u32 autoFrameSkipLastTime = 0;

int showSpeed =0;
int showSpeedTransparent = 1;
bool disableStatusMessages = false;
bool paused = false;
bool pauseNextFrame = false;
bool debugger = false;
bool debuggerStub = false;
int fullscreen = 1;
int sdlFlashSize = 1;
int sdlAutoPatch = 1;
int sdlRtcEnable = 0;
int sdlAgbPrint = 0;
int sdlMirroringEnable = 1;

static int        ignore_first_resize_event = 0;

/* forward */
void systemConsoleMessage(const char*);
void sdlApplyPerImagePreferences();

#ifndef __QNXNTO__
void (*dbgMain)() = debuggerMain;
void (*dbgSignal)(int,int) = debuggerSignal;
void (*dbgOutput)(const char *, u32) = debuggerOutput;
#endif

int  mouseCounter = 0;

bool screenMessage = false;
char screenMessageBuffer[21];
u32  screenMessageTime = 0;

char *arg0;

int g_LOADING_ROM;
extern bool stopState;  // cpu loop control

// CHEAT support
#define MAX_CHEATS    100
#define MAX_CHEAT_LEN 200
static  int  sdlPreparedCheats	= 0;
static  char sdlPreparedCheatCodes[MAX_CHEATS][MAX_CHEAT_LEN];


// OpenGL ES2.0
//-------------------------------------------------------------------------------------------------

static GLfloat vertices[]  = {-1.0,-1.0, 1.0,-1.0, -1.0,1.0, 1.0,1.0};
static GLfloat texCoords[] = {0.0,1.0, 1.0,1.0, 0.0,0.0, 1.0,0.0};
static u32     shader;
static u32     positionAttrib;
static u32     texcoordAttrib;
static u32     writableScreen;
static GLuint  textures[2];

const char *vs =
		"attribute vec2 a_position;\n"
		"attribute vec2 a_texcoord;\n"
		"varying vec2 v_texcoord;\n"
		"void main()\n"
		"{\n"
		"    gl_Position = vec4(a_position, 0.0, 1.0);\n"
		"    v_texcoord = a_texcoord;\n"
		"}\n";

const char *fs_basic =
		"uniform lowp sampler2D u_sampler;\n"
		"uniform lowp vec2  u_resolution;\n"
		"varying mediump vec2 v_texcoord;\n"
		"void main()\n"
		"{\n"
		"    gl_FragColor = texture2D(u_sampler, v_texcoord);\n"
		"}\n";

const char *fs_fxaa =
    "uniform lowp sampler2D u_sampler; // Texture0\n"
	"uniform lowp    vec2  u_resolution;\n"
    "varying mediump vec2  v_texcoord;\n"
    "\n"
    "#define FxaaInt2 ivec2\n"
    "#define FxaaFloat2 vec2\n"
    "#define FxaaTexLod0(t, p) texture2D(t, p)\n"
    "#define FxaaTexOff(t, p, o, r) texture2D(t, p + (o * r))\n"
	"#define FXAA_REDUCE_MIN   (1.0/128.0)\n"
	"#define FXAA_REDUCE_MUL   (1.0/16.0)\n"
	"#define FXAA_SPAN_MAX     2.0\n"
    "\n"
	"void main() \n"
	"{ \n"
	"    lowp vec3 c;\n"
	"    lowp float lum;"
	"    mediump vec2 rcpFrame = 1.0/u_resolution;\n"
    "/*---------------------------------------------------------*/\n"
    "/*---------------------------------------------------------*/\n"
    "    lowp vec3 rgbNW = texture2D(u_sampler, v_texcoord + (FxaaFloat2(-1.0,-1.0) * rcpFrame.xy)).xyz;\n"
    "    lowp vec3 rgbNE = texture2D(u_sampler, v_texcoord + (FxaaFloat2( 1.0,-1.0) * rcpFrame.xy)).xyz;\n"
    "    lowp vec3 rgbSW = texture2D(u_sampler, v_texcoord + (FxaaFloat2(-1.0, 1.0) * rcpFrame.xy)).xyz;\n"
    "    lowp vec3 rgbSE = texture2D(u_sampler, v_texcoord + (FxaaFloat2( 1.0, 1.0) * rcpFrame.xy)).xyz;\n"
    "    lowp vec3 rgbM  = texture2D(u_sampler, v_texcoord).xyz;\n"
    "/*---------------------------------------------------------*/\n"
    "    lowp vec3 luma = vec3(0.299, 0.587, 0.114);\n"
    "    lowp float lumaNW = dot(rgbNW, luma);\n"
    "    lowp float lumaNE = dot(rgbNE, luma);\n"
    "    lowp float lumaSW = dot(rgbSW, luma);\n"
    "    lowp float lumaSE = dot(rgbSE, luma);\n"
    "    lowp float lumaM  = dot(rgbM,  luma);\n"
    "/*---------------------------------------------------------*/\n"
    "    lowp float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));\n"
    "    lowp float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));\n"
    "/*---------------------------------------------------------*/\n"
    "    lowp vec2 dir; \n"
    "    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));\n"
    "    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));\n"
    "/*---------------------------------------------------------*/\n"
    "    lowp float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);\n"
    "    lowp float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);\n"
    "    dir = min(FxaaFloat2( FXAA_SPAN_MAX,  FXAA_SPAN_MAX), \n"
    "          max(FxaaFloat2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), \n"
    "          dir * rcpDirMin)) * rcpFrame.xy;\n"
    "/*--------------------------------------------------------*/\n"
    "    lowp vec3 rgbA = (1.0/2.0) * (\n"
    "        texture2D(u_sampler, v_texcoord.xy + dir * (1.0/3.0 - 0.5)).xyz +\n"
    "        texture2D(u_sampler, v_texcoord.xy + dir * (2.0/3.0 - 0.5)).xyz);\n"
    "    lowp vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (\n"
    "        texture2D(u_sampler, v_texcoord.xy + dir * (0.0/3.0 - 0.5)).xyz +\n"
    "        texture2D(u_sampler, v_texcoord.xy + dir * (3.0/3.0 - 0.5)).xyz);\n"
    "    lowp float lumaB = dot(rgbB, luma);\n"
    "    if((lumaB < lumaMin) || (lumaB > lumaMax))\n"
	"      c = rgbA;\n"
	"    else\n"
    "      c = rgbB;\n"
	"    gl_FragColor = vec4(c,1.0);\n"
	"}\n";

static int sdlOpenGLInit(int width, int height)
{
	GLint  status;
	GLuint v, f, id;
	GLchar log[8192];
	char  *fs;
	GLint  texfilter;

	if (shader)
		return 1;

	v = glCreateShader(GL_VERTEX_SHADER);
	if (!v) {
		SLOG("Failed to create vertex shader");
		goto error1;
	}
	glShaderSource(v, 1, &vs, 0);
	glCompileShader(v);
	glGetShaderiv(v, GL_COMPILE_STATUS, &status);
	if (GL_FALSE == status) {
		glGetShaderInfoLog(v, sizeof(log), NULL, log);
		SLOG("Failed to compile vertex shader:\n %s", log);
		goto error2;
	}

	f = glCreateShader(GL_FRAGMENT_SHADER);
	if (!f) {
		SLOG("Failed to create fragment shader");
		goto error2;
	}

	switch (g_openGL)
	{
	case 1:
		fs        = (char *)fs_basic;
		texfilter = GL_NEAREST;
		SLOG("Basic fragment shader used");
		break;
	case 2:
#ifdef STL100_1
		g_openGL  = 1;
		fs        = (char *)fs_basic;
		texfilter = GL_NEAREST;
		SLOG("Basic fragment shader used for STL100-1 device");
#else
		fs        = (char *)fs_fxaa;
		texfilter = GL_LINEAR;
		SLOG("FXAA fragment shader used");
#endif
		break;
	case 3:
	{
		int  len;
		FILE *file = fopen(SYSCONFDIR"/custom.fs", "rb");
		if (file)
		{
			fseek(file, 0, SEEK_END);
			len = ftell(file);
			fseek(file, 0, SEEK_SET);
			fs = (char *)calloc(1, len+1);
			if (fs == NULL)
			{
				SLOG("Fail allocate custom fragment shader code memory, use default");
				fs        = (char *)fs_basic;
				texfilter = GL_NEAREST;
				g_openGL  = 1;
			}
			else
			{
				fread(fs, sizeof(char), len, file);
				texfilter = GL_LINEAR;
				SLOG("Custom fragment shader used");
			}
		}
		else
		{
			SLOG("Cannot open %s, use default", SYSCONFDIR"/custom.fs");
			fs        = (char *)fs_basic;
			texfilter = GL_NEAREST;
			g_openGL  = 1;
		}
		break;
	}
	default:
		SLOG("Unsuppport OpenGL option, use default");
		fs        = (char *)fs_basic;
		texfilter = GL_NEAREST;
		g_openGL  = 1;
		break;
	}

	glShaderSource(f, 1, &fs, 0);
	glCompileShader(f);
	glGetShaderiv(f, GL_COMPILE_STATUS, &status);
	if (GL_FALSE == status) {
		glGetShaderInfoLog(f, sizeof(log), NULL, log);
		SLOG("Failed to compile fragment shader:\n %s", log);

		if (g_openGL == 3)
		{
			free (fs);
		}

		// fallback to basic fragment shader
		g_openGL  = 1;
		texfilter = GL_LINEAR;
		fs = (char *)fs_basic;
		glShaderSource(f, 1, &fs, 0);
		glCompileShader(f);
		glGetShaderiv(f, GL_COMPILE_STATUS, &status);
		if (GL_FALSE == status) {
			glGetShaderInfoLog(f, sizeof(log), NULL, log);
			SLOG("Failed to compile basic fragment shader:\n %s", log);
			goto error3;
		}
	}

	id = glCreateProgram();
	if (!id) {
		SLOG("Failed to create shader program\n");
		goto error3;
	}
	glAttachShader(id, v);
	glAttachShader(id, f);
	glLinkProgram(id);

	glGetProgramiv(id, GL_LINK_STATUS, &status);
	if (GL_FALSE == status) {
		glGetProgramInfoLog(id, sizeof(log), NULL, log);
		SLOG("Failed to link shader program:\n %s", log);
		goto error4;
	}

	glDeleteShader(v);
	glDeleteShader(f);
	if (g_openGL == 3) free(fs);

	shader = id;
	glUseProgram(id);
	glUniform1i(glGetUniformLocation(id, "u_sampler"), 0); // screen texture is TEXTURE0
	glUniform2f(glGetUniformLocation(id, "u_resolution"), (float)width, (float)height);

	positionAttrib = glGetAttribLocation(id, "a_position");
	texcoordAttrib = glGetAttribLocation(id, "a_texcoord");

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(2, textures);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texfilter);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texfilter);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texfilter);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texfilter);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	writableScreen = 0;

	return 1; // Success
error4:
	glDeleteProgram(id);
error3:
	glDeleteShader(f);
	if (g_openGL == 3) free(fs);
error2:
	glDeleteShader(v);
error1:
	return 0; // Failed to initialize
}

void updateSurface(u32 w, u32 h, u8 *pixels)
{
	glClear(GL_COLOR_BUFFER_BIT );
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[writableScreen]);
	writableScreen = writableScreen ^ 1;

	if (systemColorDepth == 16)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);
	else if (systemColorDepth == 32)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glEnableVertexAttribArray(positionAttrib);
	glEnableVertexAttribArray(texcoordAttrib);
	glVertexAttribPointer(positionAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), vertices);
	glVertexAttribPointer(texcoordAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), texCoords);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


//-------------------------------------------------------------------------------------------------

void GetRomDirListing(vector<string> &romList, vector<string> &cheatList, const char *dpath )
{
#ifdef __PLAYBOOK__
	DIR* dirp;
	struct dirent64* direntp;

	if(!dpath)
	{
		SLOG("dpath is null.\n");
		return;
	}

	dirp = opendir( dpath );
	if( dirp != NULL )
	{
		SLOG("[---- Parsing %s ----]", dpath);
		for(;;)
		{
			direntp = readdir64( dirp );
			if( direntp == NULL )
			{
				SLOG("End of readdir64");
				break;
			}

			string tmp = direntp->d_name;

			if( strcmp( direntp->d_name, ".") == 0)
			{
				continue;
			}

			if( strcmp( direntp->d_name,"..") == 0)
				continue;

			if( (tmp.substr(tmp.find_last_of(".") + 1) == "gba") ||
				(tmp.substr(tmp.find_last_of(".") + 1) == "GBA") ||
				(tmp.substr(tmp.find_last_of(".") + 1) == "gbc") ||
				(tmp.substr(tmp.find_last_of(".") + 1) == "gb")    )
			{
				tmp = dpath;
				tmp += direntp->d_name;
				SLOG("ROM: %s", tmp.c_str());
				romList.push_back(tmp);
			}
			else if(
				(tmp.substr(tmp.find_last_of(".") + 1) == "cht") ||
				(tmp.substr(tmp.find_last_of(".") + 1) == "CHT")   )
			{
				tmp = dpath;
				tmp += direntp->d_name;
				SLOG("CHEAT: %s", tmp.c_str());
				cheatList.push_back(tmp);
			}
		}
		closedir(dirp);
	}
	else
	{
		SLOG("[%s] not found ...", dpath);
	}

#endif
}




//
//
//
vector<string> sortAlpha(vector<string> sortThis)
{
	int swap;
	string temp;

	do
	{
		swap = 0;
		for (int count = 0; count < sortThis.size() - 1; count++)
		{
			if (sortThis.at(count) > sortThis.at(count + 1))
			{
				temp = sortThis.at(count);
				sortThis.at(count) = sortThis.at(count + 1);
				sortThis.at(count + 1) = temp;
				swap = 1;
			}
		}
	}while (swap != 0);

	return sortThis;
}


//
// Frame size setting: DO NOT CHANGE
//
//  GBA    -  240 x 160
//  GBC/GB -  240 x 160 including border
//  SGB    -  256 x 224 including border
//
void initGbFrameSize(void)
{
	if(cartridgeType == 0) {
		g_srcWidth  = 240;
		g_srcHeight = 160;
		systemFrameSkip = frameSkip;
		systemRenderFps = 60 - frameSkip;
	} else if (cartridgeType == 1) {
		if(gbBorderOn) {
			if (gbSgbMode)
			{
				gbWidth            = 256;
				gbHeight           = 224;
			}
			else
			{
				gbWidth            = 240;
				gbHeight           = 160;
			}
			g_srcWidth         = gbWidth;
			g_srcHeight        = gbHeight;
			gbBorderLineSkip   = gbWidth;
			gbBorderColumnSkip = (gbWidth  - GB_ACTUAL_WIDTH)/2;
			gbBorderRowSkip    = (gbHeight - GB_ACTUAL_HEIGHT)/2;
		} else {
			g_srcWidth         = GB_ACTUAL_WIDTH;
			g_srcHeight        = GB_ACTUAL_HEIGHT;
			gbBorderLineSkip   = GB_ACTUAL_WIDTH;
			gbBorderColumnSkip = 0;
			gbBorderRowSkip    = 0;
		}
		systemFrameSkip = gbFrameSkip;
		systemRenderFps = 60 - gbFrameSkip;
	} else {
		g_srcWidth  = 320;
		g_srcHeight = 240;
	}
}

int AutoLoadRom(void)
{
	static int    load_in_progress = 0;
	string        romfilename;
	bbDialog     *dialog;
	const char  **list = 0;
	int           count;

	extern void sdlWriteBattery();
	extern void sdlReadBattery();

	sdlWriteBattery();

	pthread_mutex_lock(&loader_mutex);

	list = (const char**)malloc(sortedRomList.size()*sizeof(char*));

	// Create Dialog file list, only the filename without the FULL path
	for(count=0; count < sortedRomList.size(); count++)
	{
		romfilename = sortedRomList.at(count);
		list[count] = sortedRomList[count].c_str() + romfilename.find_last_of("/") + 1;
	}

	dialog = new bbDialog;
	if (dialog)
	{
		gameIndex = dialog->showPopuplistDialog(list, sortedRomList.size(), "ROM Selector");
		delete dialog;
		free(list);

		if (gameIndex < 0)
		{
			g_LOADING_ROM    = 0;
			stopState        = false;
			load_in_progress = 0;

			pthread_mutex_unlock(&loader_mutex);

			return false;
		}
		else
			romfilename = sortedRomList[gameIndex];
	}

	//test end

	SLOG("AutoLoadRom\n");

	memset(&g_runningFile_str[0],0,64);
	sprintf(&g_runningFile_str[0], romfilename.c_str());

	SLOG("loading: %d/%d '%s'\n",gameIndex + 1, sortedRomList.size(), romfilename.c_str() );
	strcpy(filename, romfilename.c_str());


	int size = 0;

	if(load_in_progress)
	{
		g_LOADING_ROM    = 0;
		stopState        = false;
		load_in_progress = 0;

		pthread_mutex_unlock(&loader_mutex);

		return false;
	}

	g_LOADING_ROM    = 1;
	stopState        = true;
	load_in_progress = 1;

	// free dynamic allocated mem etc of current ROM
	if (cartridgeType == IMAGE_GBA)
	{
		CPUCleanUp();
	}
	else if (cartridgeType == IMAGE_GB)
	{
		gbCleanUp();
	}

	IMAGE_TYPE type = utilFindType(filename);

	SLOG("'%s' ROM type = %d\n",filename, type);

	if(type == IMAGE_UNKNOWN) {
		systemMessage(MSG_CANNOT_OPEN_FILE, "Unknown file type %s", filename);
		g_LOADING_ROM    = 0;
		stopState        = false;
		load_in_progress = 0;

		pthread_mutex_unlock(&loader_mutex);

		return false;
	}
	cartridgeType = (int)type;

	if (cartridgeType == IMAGE_GB)
	{
		SLOG("GB image '%s'\n",filename);
		if ( (size = gbLoadRom(filename)) == 0)
		{
			SLOG("ERROR loading %s\n",filename);
			g_LOADING_ROM    = 0;
			stopState        = false;
			load_in_progress = 0;

			pthread_mutex_unlock(&loader_mutex);

			return false;
		}
		gbGetHardwareType();

		// used for the handling of the gb Boot Rom
		if (gbHardware & 5)
			gbCPUInit(gbBiosFileName, useBios);

		emulator = GBSystem;

		SLOG("gbReset");
		gbReset();
	}
	else if (cartridgeType == IMAGE_GBA)
	{
		SLOG("GB image '%s'\n", filename);
		if( (size=CPULoadRom(filename)) == 0)
		{
			SLOG("ERROR loading %s\n",filename);
			g_LOADING_ROM    = 0;
			stopState        = false;
			load_in_progress = 0;

			pthread_mutex_unlock(&loader_mutex);

			return false;
		}

		sdlApplyPerImagePreferences();
		doMirroring(mirroringEnable);

		emulator = GBASystem;

		SLOG("CPUInit '%s' %d", biosFileName, useBios);

		useBios = false;
		CPUInit(biosFileName, useBios);

		SLOG("CPUReset");

		CPUReset();
	}

	initGbFrameSize();
	sdlInitVideo();

	g_LOADING_ROM    = 0;
	stopState        = false;
	load_in_progress = 0;

	SLOG("RomLoad: ok\n");
	sdlReadBattery();

	systemFrameInit();

	pthread_mutex_unlock(&loader_mutex);
	//drawText(screen, destPitch, 40, 150, szFile, 0);
	return true;
}

void UpdateRomList(void)
{
	SLOG("Obtain Device ROM list and CHEAT list...");
	GetRomDirListing(romList, cheatList, SYSROMDIR);

	SLOG("Obtain SD-Card ROM list and CHEAT list...");
	GetRomDirListing(romList, cheatList, SDCARDROMDIR);

	SLOG("Total ROMS found: %d", romList.size() );
	SLOG("Total CHEATS found: %d", cheatList.size() );
	if (romList.size() > 0)
	{
		sortedRomList = sortAlpha(romList);

		if (cheatList.size() > 0)
		{
			sortedCheatList = sortAlpha(cheatList);
		}
	}
	else
	{
		bbDialog *dialog = new (bbDialog);

		if (dialog)
		{
			dialog->showAlert("GBAEMU Error Report", "ERROR: You do not have any ROMS! Add GBA BIOS & ROMS to:\"misc/roms/gba\"");
			delete dialog;
		}

		exit(-1);
	}
}

bool showCheatSelectionDialog(void)
{
	const char  **list = 0;
	int           count = 0;
	int           cheatListIdx;
	string        cheatfilename;
	bbDialog     *dialog;

	if (sortedCheatList.size() == 0)
	{
		dialog = new bbDialog;
		if (dialog)
		{
			dialog->showAlert(
					"Cheat File Not Found",
					"Please place cheat files (.CHT) in the ROM folder.\n"
					"Format is based on Gameshark / Action Replay, support V1/V3.\n"
					"First line must indicate V1/V3, rest is code/comments, e.g.\n"
					"\nV3\n"
					"// Pokemon Emerald patches\n"
					"// Master Code - must be ON\n"
					"B749822B CE9BFAC1\n"
					"A86CDBA5 19BA49B3\n"
					"// Pikachu - Wild pokemon modifier\n"
					"BC7EC610 A4B1CCA6");
		}
		return false;
	}

	list = (const char**)malloc(sortedCheatList.size()*sizeof(char*));

	// ROM selection
	if (list)
	{
		for(count = 0; count < sortedCheatList.size(); count++)
		{
			cheatfilename = sortedCheatList.at(count);
			list[count]   = sortedCheatList[count].c_str() + cheatfilename.find_last_of("/") + 1;
		}

		dialog = new bbDialog;

		if (dialog)
		{
			cheatListIdx = dialog->showPopuplistDialog(list, sortedCheatList.size(), "CHEAT selector");
			delete dialog;
			free(list);

			if (cheatListIdx >= 0)
			{
				cheatfilename = sortedCheatList.at(cheatListIdx);
			}
			else
			{
				SLOG("Bad selection index from Popup List Dialog");
				return false;
			}
		}
		else
		{
			SLOG("Fail creating BB dialog, quiting");
			return false;
		}
	}
	else
	{
		SLOG("Out of Memory, Fail creating CHEAT list, quiting!!");
		return false;
	}

	sdlPreparedCheats = 0;

	FILE *fIn = fopen(cheatfilename.c_str(), "r");
	if (fIn)
	{
		while(fgets(sdlPreparedCheatCodes[sdlPreparedCheats++], MAX_CHEAT_LEN-1, fIn));

		int i;
		bool isV3 = true;

		if (sdlPreparedCheats > 0)
		{
			cheatsDeleteAll(false);
		}

		for (i=0; i<sdlPreparedCheats; i++) {
			char *p;
			int	  l;
			p	= sdlPreparedCheatCodes[i];
			l	= strlen(p);

			// skip comments
			if(p[0] == '#' || p[0] == '/') continue;
			else if (i==0 && p[0] == 'V' && p[1] == '1')  isV3 = false;
			else if (i==0 && p[0] == 'V' && p[1] == '3')  isV3 = true;

			// elimiate Carriage return and linefeed char
			while (l> 0 && (p[l-1] == '\n' || p[l-1] == '\r'))
			{
				p[l-1] = '\0';
				--l;
			}

			if (l == 17 && p[8] == ' ') {
				SLOG("Adding cheat code %s", p);
				cheatsAddGSACode(p, p, isV3);
			} else if (l == 13 && p[8] == ' ') {
				SLOG("Adding CBA cheat code %s", p);
				cheatsAddCBACode(p, p);
			} else if (l == 8) {
				SLOG("Adding GB(GS) cheat code %s", p);
				gbAddGsCheat(p, p);
			}
		}

		fclose(fIn);
	}

	return (sdlPreparedCheats > 0) ? true : false;
}

#define SDL_SOUND_MAX_VOLUME 2.0
#define SDL_SOUND_ECHO       0.2
#define SDL_SOUND_STEREO     0.15

struct option sdlOptions[] = {
	{ "agb-print", no_argument, &sdlAgbPrint, 1 },
	{ "auto-frameskip", no_argument, &autoFrameSkip, 1 },
	{ "bios", required_argument, 0, 'b' },
	{ "config", required_argument, 0, 'c' },
	{ "debug", no_argument, 0, 'd' },
	{ "filter", required_argument, 0, 'f' },
	{ "ifb-filter", required_argument, 0, 'I' },
	{ "flash-size", required_argument, 0, 'S' },
	{ "flash-64k", no_argument, &sdlFlashSize, 0 },
	{ "flash-128k", no_argument, &sdlFlashSize, 1 },
	{ "frameskip", required_argument, 0, 's' },
	{ "fullscreen", no_argument, &fullscreen, 1 },
	{ "gdb", required_argument, 0, 'G' },
	{ "help", no_argument, &sdlPrintUsage, 1 },
	{ "patch", required_argument, 0, 'i' },
	{ "no-agb-print", no_argument, &sdlAgbPrint, 0 },
	{ "no-auto-frameskip", no_argument, &autoFrameSkip, 0 },
	{ "no-debug", no_argument, 0, 'N' },
	{ "no-patch", no_argument, &sdlAutoPatch, 0 },
	{ "no-opengl", no_argument, &g_openGL, 0 },
	{ "no-pause-when-inactive", no_argument, &pauseWhenInactive, 0 },
	{ "no-rtc", no_argument, &sdlRtcEnable, 0 },
	{ "no-show-speed", no_argument, &showSpeed, 0 },
	{ "opengl", required_argument, 0, 'O' },
	{ "opengl-nearest", no_argument, &g_openGL, 1 },
	{ "opengl-bilinear", no_argument, &g_openGL, 2 },
	{ "pause-when-inactive", no_argument, &pauseWhenInactive, 1 },
	{ "profile", optional_argument, 0, 'p' },
	{ "rtc", no_argument, &sdlRtcEnable, 1 },
	{ "save-type", required_argument, 0, 't' },
	{ "save-auto", no_argument, &cpuSaveType, 0 },
	{ "save-eeprom", no_argument, &cpuSaveType, 1 },
	{ "save-sram", no_argument, &cpuSaveType, 2 },
	{ "save-flash", no_argument, &cpuSaveType, 3 },
	{ "save-sensor", no_argument, &cpuSaveType, 4 },
	{ "save-none", no_argument, &cpuSaveType, 5 },
	{ "show-speed-normal", no_argument, &showSpeed, 1 },
	{ "show-speed-detailed", no_argument, &showSpeed, 2 },
	{ "throttle", required_argument, 0, 'T' },
	{ "verbose", required_argument, 0, 'v' },
	{ "cheat", required_argument, 0, 1000 },
	{ "autofire", required_argument, 0, 1001 },
	{ NULL, no_argument, NULL, 0 }
};

static void sdlChangeVolume(float d)
{
	float oldVolume = soundGetVolume();
	float newVolume = oldVolume + d;

	if (newVolume < 0.0) newVolume = 0.0;
	if (newVolume > SDL_SOUND_MAX_VOLUME) newVolume = SDL_SOUND_MAX_VOLUME;

#ifdef __QNXNTO__
	if((newVolume - oldVolume) > 0.001) {
#else
		if (fabs(newVolume - oldVolume) > 0.001) {
#endif
			char tmp[32];
			sprintf(tmp, "Volume: %i%%", (int)(newVolume*100.0+0.5));
			systemScreenMessage(tmp);
			soundSetVolume(newVolume);
		}
	}

#if WITH_LIRC
//LIRC code
bool LIRCEnabled = false;
int  LIRCfd = 0;
static struct lirc_config *LIRCConfigInfo;

void StartLirc(void)
{
	SLOG( "Trying to start LIRC: ");
	//init LIRC and Record output
	LIRCfd = lirc_init( "vbam",1 );
	if( LIRCfd == -1 ) {
		//it failed
		SLOG( "Failed\n");
	} else {
		SLOG( "Success\n");
		//read the config file
		char LIRCConfigLoc[2048];
		sprintf(LIRCConfigLoc, "%s/%s/%s", homeDir, DOT_DIR, "lircrc");
		SLOG( "LIRC Config file:");
		if( lirc_readconfig(LIRCConfigLoc,&LIRCConfigInfo,NULL) == 0 ) {
			//check vbam dir for lircrc
			SLOG( "Loaded (%s)\n", LIRCConfigLoc );
		} else if( lirc_readconfig(NULL,&LIRCConfigInfo,NULL) == 0 ) {
			//check default lircrc location
			SLOG( "Loaded\n");
		} else {
			//it all failed
			SLOG( "Failed\n");
			LIRCEnabled = false;
		}
		LIRCEnabled = true;
	}
}

void StopLirc(void)
{
	//did we actually get lirc working at the start
	if(LIRCEnabled) {
		//if so free the config and deinit lirc
		SLOG( "Shuting down LIRC\n");
		lirc_freeconfig(LIRCConfigInfo);
		lirc_deinit();
		//set lirc enabled to false
		LIRCEnabled = false;
	}
}
#endif


u32 sdlFromHex(char *s)
{
	u32 value;
	sscanf(s, "%x", &value);
	return value;
}

u32 sdlFromDec(char *s)
{
	u32 value = 0;
	sscanf(s, "%u", &value);
	return value;
}

#ifdef __MSC__
#define stat _stat
#define S_IFDIR _S_IFDIR
#endif

void sdlCheckDirectory(char *dir)
{
	struct stat buf;

	int len = strlen(dir);

	char *p = dir + len - 1;

	if(*p == '/' ||
			*p == '\\')
		*p = 0;

	if(stat(dir, &buf) == 0) {
		if(!(buf.st_mode & S_IFDIR)) {
			SLOG( "Error: %s is not a directory\n", dir);
			dir[0] = 0;
		}
	} else {
		SLOG( "Error: %s does not exist\n", dir);
		dir[0] = 0;
	}
}

char *sdlGetFilename(char *name)
{
	static char filebuffer[2048];

	int len = strlen(name);

	char *p = name + len - 1;

	while(true) {
		if(*p == '/' ||
				*p == '\\') {
			p++;
			break;
		}
		len--;
		p--;
		if(len == 0)
			break;
	}

	if(len == 0)
		strcpy(filebuffer, name);
	else
		strcpy(filebuffer, p);
	return filebuffer;
}

FILE *sdlFindFile(const char *name)
{
	char buffer[4096];
	char path[2048];
	SLOG("sdlFindFile: %s\n", name);

	FILE *f = fopen(name, "r");
	if(f != NULL) {
		return f;
	}
#ifdef _WIN32
#define PATH_SEP ";"
#define FILE_SEP '\\'
#define EXE_NAME "vbam.exe"
#else // ! _WIN32
#define PATH_SEP ":"
#define FILE_SEP '/'
#define EXE_NAME "vbam"
#endif // ! _WIN32

	SLOG( "Searching for file %s\n", name);

	if(GETCWD(buffer, 2048)) {
		SLOG( "Searching current directory: %s\n", buffer);
	}

	f = fopen(name, "r");
	if(f != NULL) {
		return f;
	}

	if(homeDir) {
		SLOG( "Searching home directory: %s%c%s\n", homeDir, FILE_SEP, DOT_DIR);
		sprintf(path, "%s%c%s%c%s", homeDir, FILE_SEP, DOT_DIR, FILE_SEP, name);
		f = fopen(path, "r");
		if(f != NULL)
			return f;
	}

#ifdef _WIN32
	char *home = getenv("USERPROFILE");
	if(home != NULL) {
		SLOG( "Searching user profile directory: %s\n", home);
		sprintf(path, "%s%c%s", home, FILE_SEP, name);
		f = fopen(path, "r");
		if(f != NULL)
			return f;
	}
#else // ! _WIN32
	SLOG( "Searching system config directory: %s\n", SYSCONFDIR);
	sprintf(path, "%s%c%s", SYSCONFDIR, FILE_SEP, name);
	f = fopen("vbam.cfg", "r");
	if(f != NULL)
	{
		SLOG("loading vbam.cfg\n");
		return f;
	}
#endif // ! _WIN32

	if(!strchr(arg0, '/') &&
			!strchr(arg0, '\\')) {
		char *path = getenv("PATH");

		if(path != NULL) {
			SLOG( "Searching PATH\n");
			strncpy(buffer, path, 4096);
			buffer[4095] = 0;
			char *tok = strtok(buffer, PATH_SEP);

			while(tok) {
				sprintf(path, "%s%c%s", tok, FILE_SEP, EXE_NAME);
				f = fopen(path, "r");
				if(f != NULL) {
					char path2[2048];
					fclose(f);
					sprintf(path2, "%s%c%s", tok, FILE_SEP, name);
					f = fopen(path2, "r");
					if(f != NULL) {
						SLOG( "Found at %s\n", path2);
						return f;
					}
				}
				tok = strtok(NULL, PATH_SEP);
			}
		}
	} else {
		// executable is relative to some directory
		SLOG( "Searching executable directory\n");
		strcpy(buffer, arg0);
		char *p = strrchr(buffer, FILE_SEP);
		if(p) {
			*p = 0;
			sprintf(path, "%s%c%s", buffer, FILE_SEP, name);
			f = fopen(path, "r");
			if(f != NULL)
				return f;
		}
	}
	return NULL;
}

void sdlReadPreferences(FILE *f)
{
	char buffer[2048];

	if(!f)
		return;

	while(1) {
		char *s = fgets(buffer, 2048, f);

		if(s == NULL)
			break;

		char *p  = strchr(s, '#');

		if(p)
			*p = 0;

		char *token = strtok(s, " \t\n\r=");

		if(!token)
			continue;

		if(strlen(token) == 0)
			continue;

		char *key = token;
		char *value = strtok(NULL, "\t\n\r");

		if(value == NULL) {
			SLOG( "Empty value for key %s\n", key);
			continue;
		}

		if(!strcmp(key,"Joy0_Left")) {
			inputSetKeymap(PAD_1, KEY_LEFT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Right")) {
			inputSetKeymap(PAD_1, KEY_RIGHT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Up")) {
			inputSetKeymap(PAD_1, KEY_UP, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Down")) {
			inputSetKeymap(PAD_1, KEY_DOWN, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Down2")) {
			inputSetKeymap(PAD_1, KEY_DOWN2, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_A")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_B")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_L")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_L, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_R")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_R, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Start")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_START, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Select")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_SELECT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Speed")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_SPEED, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Capture")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_CAPTURE, sdlFromHex(value));
		} else if(!strcmp(key,"Joy1_Left")) {
			inputSetKeymap(PAD_2, KEY_LEFT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Right")) {
			inputSetKeymap(PAD_2, KEY_RIGHT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Up")) {
			inputSetKeymap(PAD_2, KEY_UP, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Down")) {
			inputSetKeymap(PAD_2, KEY_DOWN, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_A")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_B")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_L")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_L, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_R")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_R, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Start")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_START, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Select")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_SELECT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Speed")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_SPEED, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Capture")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_CAPTURE, sdlFromHex(value));
		} else if(!strcmp(key,"Joy2_Left")) {
			inputSetKeymap(PAD_3, KEY_LEFT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Right")) {
			inputSetKeymap(PAD_3, KEY_RIGHT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Up")) {
			inputSetKeymap(PAD_3, KEY_UP, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Down")) {
			inputSetKeymap(PAD_3, KEY_DOWN, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_A")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_B")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_L")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_L, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_R")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_R, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Start")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_START, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Select")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_SELECT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Speed")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_SPEED, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Capture")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_CAPTURE, sdlFromHex(value));
		} else if(!strcmp(key,"Joy3_Left")) {
			inputSetKeymap(PAD_4, KEY_LEFT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Right")) {
			inputSetKeymap(PAD_4, KEY_RIGHT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Up")) {
			inputSetKeymap(PAD_4, KEY_UP, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Down")) {
			inputSetKeymap(PAD_4, KEY_DOWN, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_A")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_B")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_L")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_L, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_R")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_R, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Start")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_START, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Select")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_SELECT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Speed")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_SPEED, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Capture")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_CAPTURE, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_AutoA")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_AUTO_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_AutoB")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_AUTO_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_AutoA")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_AUTO_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_AutoB")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_AUTO_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_AutoA")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_AUTO_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_AutoB")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_AUTO_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_AutoA")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_AUTO_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_AutoB")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_AUTO_B, sdlFromHex(value));
		} else if(!strcmp(key, "openGL")) {
			g_openGL = sdlFromHex(value);
		} else if(!strcmp(key, "logToFile")) {
			g_logtofile = sdlFromHex(value);
			if (g_logtofile) dbgDialog = new (bbDialog);
			g_flogfile = fopen(SYSCONFDIR"/logs", "w");
			if (g_flogfile == NULL) g_logtofile = 0;
		} else if(!strcmp(key, "Motion_Left")) {
			inputSetMotionKeymap(KEY_LEFT, sdlFromHex(value));
		} else if(!strcmp(key, "Motion_Right")) {
			inputSetMotionKeymap(KEY_RIGHT, sdlFromHex(value));
		} else if(!strcmp(key, "Motion_Up")) {
			inputSetMotionKeymap(KEY_UP, sdlFromHex(value));
		} else if(!strcmp(key, "Motion_Down")) {
			inputSetMotionKeymap(KEY_DOWN, sdlFromHex(value));
		} else if(!strcmp(key, "frameSkip")) {
			frameSkip = sdlFromHex(value);
			if(frameSkip < 0 || frameSkip > 9)
				frameSkip = 2;
		} else if(!strcmp(key, "gbFrameSkip")) {
			gbFrameSkip = sdlFromHex(value);
			if(gbFrameSkip < 0 || gbFrameSkip > 9)
				gbFrameSkip = 0;
		} else if(!strcmp(key, "fullScreen")) {
			fullscreen = sdlFromHex(value) ? 1 : 0;
		} else if(!strcmp(key, "useBios")) {
			useBios = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "skipBios")) {
			skipBios = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "biosFile")) {
			strcpy(biosFileName, value);
		} else if(!strcmp(key, "gbBiosFile")) {
			strcpy(gbBiosFileName, value);
		} else if(!strcmp(key, "filter")) {
			filter = (Filter)sdlFromDec(value);
			if(filter < kStretch1x || filter >= kInvalidFilter)
				filter = kStretch1x;
		} else if(!strcmp(key, "disableStatus")) {
			disableStatusMessages = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "borderOn")) {
			gbBorderOn = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "borderAutomatic")) {
			gbBorderAutomatic = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "emulatorType")) {
			gbEmulatorType = sdlFromHex(value);
			if(gbEmulatorType < 0 || gbEmulatorType > 5)
				gbEmulatorType = 1;
		} else if(!strcmp(key, "colorOption")) {
			gbColorOption = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "captureDir")) {
			sdlCheckDirectory(value);
			strcpy(captureDir, value);
		} else if(!strcmp(key, "saveDir")) {
			sdlCheckDirectory(value);
			strcpy(saveDir, value);
		} else if(!strcmp(key, "batteryDir")) {
			sdlCheckDirectory(value);
			strcpy(batteryDir, value);
		} else if(!strcmp(key, "captureFormat")) {
			captureFormat = sdlFromHex(value);
		} else if(!strcmp(key, "soundQuality")) {
			systemSoundQuality = sdlFromHex(value);
			switch(systemSoundQuality) {
			case 0:
			case 1:
			case 2:
			case 4:
				break;
			default:
				SLOG( "Unknown sound quality %d. Defaulting to 22Khz\n", systemSoundQuality);
				systemSoundQuality = 2;
				break;
			}
		} else if(!strcmp(key, "soundEnable")) {
			systemSoundRes = sdlFromHex(value) & 0x30f;
		} else if(!strcmp(key, "soundStereo")) {
			if (sdlFromHex(value)) {
				gb_effects_config.stereo = SDL_SOUND_STEREO;
				gb_effects_config.enabled = true;
			}
		} else if(!strcmp(key, "soundEcho")) {
			if (sdlFromHex(value)) {
				gb_effects_config.echo = SDL_SOUND_ECHO;
				gb_effects_config.enabled = true;
			}
		} else if(!strcmp(key, "soundSurround")) {
			if (sdlFromHex(value)) {
				gb_effects_config.surround = true;
				gb_effects_config.enabled = true;
			}
		} else if(!strcmp(key, "declicking")) {
			systemSoundDeclicking = sdlFromHex(value);
		} else if(!strcmp(key, "soundVolume")) {
			float volume = sdlFromDec(value) / 100.0;
			if (volume < 0.0 || volume > SDL_SOUND_MAX_VOLUME)
				volume = 1.0;
			soundSetVolume(volume);
		} else if(!strcmp(key, "saveType")) {
			cpuSaveType = sdlFromHex(value);
			if(cpuSaveType < 0 || cpuSaveType > 5)
				cpuSaveType = 0;
		} else if(!strcmp(key, "flashSize")) {
			// sdlFlashSize = sdlFromHex(value);
			sdlFlashSize = 1;
			if(sdlFlashSize != 0 && sdlFlashSize != 1)
				sdlFlashSize = 0;
		} else if(!strcmp(key, "ifbType")) {
			ifbType = (IFBFilter)sdlFromHex(value);
			if(ifbType < kIFBNone || ifbType >= kInvalidIFBFilter)
				ifbType = kIFBNone;
		} else if(!strcmp(key, "showSpeed")) {
			showSpeed = sdlFromHex(value);
			if(showSpeed < 0 || showSpeed > 2)
				showSpeed = 1;
		} else if(!strcmp(key, "showSpeedTransparent")) {
			showSpeedTransparent = sdlFromHex(value);
		} else if(!strcmp(key, "autoFrameSkip")) {
			autoFrameSkip = sdlFromHex(value);
		} else if(!strcmp(key, "pauseWhenInactive")) {
			pauseWhenInactive = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "agbPrint")) {
			sdlAgbPrint = sdlFromHex(value);
		} else if(!strcmp(key, "rtcEnabled")) {
			sdlRtcEnable = sdlFromHex(value);
		} else if(!strcmp(key, "rewindTimer")) {
			rewindTimer = sdlFromHex(value);
			if(rewindTimer < 0 || rewindTimer > 600)
				rewindTimer = 0;
			rewindTimer *= 6;  // convert value to 10 frames multiple
		} else if(!strcmp(key, "saveKeysSwitch")) {
			sdlSaveKeysSwitch = sdlFromHex(value);
		} else if(!strcmp(key, "openGLscale")) {
			sdlOpenglScale = sdlFromHex(value);
		} else if(!strcmp(key, "autoFireMaxCount")) {
			autoFireMaxCount = sdlFromDec(value);
			if(autoFireMaxCount < 1)
				autoFireMaxCount = 1;
		} else {
			SLOG( "Unknown configuration key %s\n", key);
		}
	}
}


void sdlReadPreferences()
{
	//  FILE *f = sdlFindFile("vbam.cfg");

	mkdir(SYSCONFDIR, 0777);
	chmod(SYSCONFDIR, 0777);

	FILE *f = fopen(SYSCONFDIR"/vbam.cfg","r");

	if(f == NULL) {
		SLOG( SYSCONFDIR"/vbam.cfg not found... \n");
		f = fopen("vbam_default.cfg","r");
		if(f == NULL)
		{
			SLOG("./vbam_default.cfg failed to load ...\n");
		}
		return;
	} else
		SLOG( "Reading configuration file ../shared/misc/gbaemu/vbam.cfg \n");

	sdlReadPreferences(f);

	fclose(f);
}

void sdlApplyPerImagePreferences()
{
	FILE *f = sdlFindFile("/accounts/1000/shared/misc/gbaemu/vbam-over.ini");
	if(!f) {
		SLOG( "vba-over.ini NOT FOUND (using emulator settings)\n");
		return;
	} else
		SLOG( "Reading vba-over.ini\n");

	char buffer[7];
	buffer[0] = '[';
	buffer[1] = rom[0xac];
	buffer[2] = rom[0xad];
	buffer[3] = rom[0xae];
	buffer[4] = rom[0xaf];
	buffer[5] = ']';
	buffer[6] = 0;

	SLOG("Loaded Game token <%s>", buffer);

	char readBuffer[2048];

	bool found = false;

	while(1) {
		char *s = fgets(readBuffer, 2048, f);

		if(s == NULL)
			break;

		char *p  = strchr(s, ';');

		if(p)
			*p = 0;

		char *token = strtok(s, " \t\n\r=");

		if(!token)
			continue;
		if(strlen(token) == 0)
			continue;

		if(!strcmp(token, buffer)) {
			SLOG("Found Game token <%s>", buffer);
			found = true;
			break;
		}
	}

	if(found) {
		while(1) {
			char *s = fgets(readBuffer, 2048, f);

			if(s == NULL)
				break;

			char *p = strchr(s, ';');
			if(p)
				*p = 0;

			char *token = strtok(s, " \t\n\r=");
			if(!token)
				continue;
			if(strlen(token) == 0)
				continue;

			if(token[0] == '[') // starting another image settings
				break;
			char *value = strtok(NULL, "\t\n\r=");
			if(value == NULL)
				continue;

			if(!strcmp(token, "rtcEnabled"))
				rtcEnable(atoi(value) == 0 ? false : true);
			else if(!strcmp(token, "flashSize")) {
				int size = atoi(value);
				if(size == 0x10000 || size == 0x20000)
					flashSetSize(size);
			} else if(!strcmp(token, "saveType")) {
				int save = atoi(value);
				if(save >= 0 && save <= 5)
					cpuSaveType = save;
			} else if(!strcmp(token, "mirroringEnabled")) {
				mirroringEnable = (atoi(value) == 0 ? false : true);
			}
		}
	}
	fclose(f);
}

static int sdlCalculateShift(u32 mask)
{
	int m = 0;

	while(mask) {
		m++;
		mask >>= 1;
	}

	return m-5;
}

/* returns filename of savestate num, in static buffer (not reentrant, no need to free,
 * but value won't survive much - so if you want to remember it, dup it)
 * You may use the buffer for something else though - until you call sdlStateName again
 */
static char * sdlStateName(int num)
{
	static char stateName[2048];

	if(saveDir[0])
		sprintf(stateName, "%s/%s%d.sgm", saveDir, sdlGetFilename(filename), num+1);
	else if (homeDir)
		sprintf(stateName, "%s/%s/%s%d.sgm", homeDir, DOT_DIR, sdlGetFilename(filename), num + 1);
	else
		sprintf(stateName,"%s%d.sgm", filename, num+1);

	SLOG("stateName:%s", stateName);

	return stateName;
}

static int sdlStateWriteFlag = 0;

void sdlWriteState(int num)
{
	char * stateName;

	stateName = sdlStateName(num);

	if(emulator.emuWriteState)
	{
		sdlStateWriteFlag = 1;
		emulator.emuWriteState(stateName);
	}

	systemScreenMessage("State Backup");

	systemFrameInit();
	systemDrawScreen();
}

void sdlReadState(int num)
{
	char * stateName;

	stateName = sdlStateName(num);

	if(emulator.emuReadState)
		emulator.emuReadState(stateName);

	if (sdlStateWriteFlag == 0)
	{
		systemScreenMessage("State Loaded");
	}

	systemFrameInit();
	systemDrawScreen();

	sdlStateWriteFlag = 0;
}

/*
 * perform savestate exchange
 * - put the savestate in slot "to" to slot "backup" (unless backup == to)
 * - put the savestate in slot "from" to slot "to" (unless from == to)
 */
void sdlWriteBackupStateExchange(int from, int to, int backup)
{
	char * dmp;
	char * stateNameOrig	= NULL;
	char * stateNameDest	= NULL;
	char * stateNameBack	= NULL;

	dmp		= sdlStateName(from);
	stateNameOrig = (char*)realloc(stateNameOrig, strlen(dmp) + 1);
	strcpy(stateNameOrig, dmp);
	dmp		= sdlStateName(to);
	stateNameDest = (char*)realloc(stateNameDest, strlen(dmp) + 1);
	strcpy(stateNameDest, dmp);
	dmp		= sdlStateName(backup);
	stateNameBack = (char*)realloc(stateNameBack, strlen(dmp) + 1);
	strcpy(stateNameBack, dmp);

	/* on POSIX, rename would not do anything anyway for identical names, but let's check it ourselves anyway */
	if (to != backup) {
		if (-1 == rename(stateNameDest, stateNameBack)) {
			SLOG( "savestate backup: can't backup old state %s to %s", stateNameDest, stateNameBack );
			perror(": ");
		}
	}
	if (to != from) {
		if (-1 == rename(stateNameOrig, stateNameDest)) {
			SLOG( "savestate backup: can't move new state %s to %s", stateNameOrig, stateNameDest );
			perror(": ");
		}
	}

	systemConsoleMessage("Savestate store and backup committed"); // with timestamp and newline
	SLOG( "to slot %d, backup in %d, using temporary slot %d\n", to+1, backup+1, from+1);

	free(stateNameOrig);
	free(stateNameDest);
	free(stateNameBack);
}

void sdlWriteBattery()
{
	char buffer[1048];

	if(batteryDir[0])
		sprintf(buffer, "%s/%s.sav", batteryDir, sdlGetFilename(filename));
	else if (homeDir)
		sprintf(buffer, "%s/%s/%s.sav", homeDir, DOT_DIR, sdlGetFilename(filename));
	else
		sprintf(buffer, "%s.sav", filename);

	SLOG("Save name: %s", buffer);

	emulator.emuWriteBattery(buffer);

	SLOG("Battery save");

}

void sdlReadBattery()
{
	char buffer[1048];

	if(batteryDir[0])
		sprintf(buffer, "%s/%s.sav", batteryDir, sdlGetFilename(filename));
	else if (homeDir)
		sprintf(buffer, "%s/%s/%s.sav", homeDir, DOT_DIR, sdlGetFilename(filename));
	else
		sprintf(buffer, "%s.sav", filename);

	bool res = false;

	SLOG("Save name: %s", buffer);

	res = emulator.emuReadBattery(buffer);

	if(res)
		SLOG("Loaded battery");
}

void sdlReadDesktopVideoMode() {
	const SDL_VideoInfo* vInfo = SDL_GetVideoInfo();
	g_desktopWidth  = vInfo->current_w;  // 1024 for pb
	g_desktopHeight = vInfo->current_h;  // 600  for pb

}

void sdlInitVideo() {
	int flags;
	int screenWidth;
	int screenHeight;

	filter_enlarge = getFilterEnlargeFactor(filter);

	g_destWidth  = filter_enlarge * g_srcWidth;
	g_destHeight = filter_enlarge * g_srcHeight;

	flags = SDL_ANYFORMAT | (fullscreen ? SDL_FULLSCREEN : 0);
	if(g_openGL) {
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		flags |= SDL_OPENGL | SDL_RESIZABLE;
	} else
		flags |= SDL_HWSURFACE | SDL_DOUBLEBUF;

	if (g_openGL) {
		screenWidth  = 480;
		screenHeight = 320;
	} else {
		screenWidth  = g_destWidth;
		screenHeight = g_destHeight;
	}

	SLOG("video size: %d x %d", screenWidth, screenHeight);

#ifdef GBA_USE_RGBA8888
	g_surface = SDL_SetVideoMode(screenWidth, screenHeight, 32, flags);
#else
	g_surface = SDL_SetVideoMode(screenWidth, screenHeight, 16, flags);
#endif

	if(g_surface == NULL) {
		SLOG("g_surface is NULL!\n");
		SLOG("Failed to set video mode");
		SDL_Quit();
		exit(-1);
	}
	SLOG("SDL video mode set ...");
	SDL_ShowCursor(0);

	systemRedShift   = sdlCalculateShift(g_surface->format->Rmask);
	systemGreenShift = sdlCalculateShift(g_surface->format->Gmask);
	systemBlueShift  = sdlCalculateShift(g_surface->format->Bmask);

	systemColorDepth = g_surface->format->BitsPerPixel;

	if(systemColorDepth == 16) {
		g_srcPitch = g_srcWidth*2;
	} else {
		if(systemColorDepth == 32)
			g_srcPitch = g_srcWidth*4;
		else
			g_srcPitch = g_srcWidth*3;
	}

#ifndef NO_OGL
	if(g_openGL) {
		int scaledWidth  = screenWidth  * sdlOpenglScale;
		int scaledHeight = screenHeight * sdlOpenglScale;

		if (filterPix)
			free(filterPix);

		if (	(!fullscreen)
				&&	sdlOpenglScale	> 1
				&&	scaledWidth	< g_desktopWidth
				&&	scaledHeight	< g_desktopHeight
		) {
			SDL_SetVideoMode(scaledWidth, scaledHeight, 0,
					SDL_OPENGL | SDL_RESIZABLE |
					(fullscreen ? SDL_FULLSCREEN : 0));
			sdlOpenGLInit(scaledWidth, scaledWidth);
			/* xKiv: it would seem that SDL_RESIZABLE causes the *previous* dimensions to be immediately
			 * reported back via the SDL_VIDEORESIZE event
			 */
			ignore_first_resize_event	= 1;
		}
		else
		{
			sdlOpenGLInit(screenWidth, screenHeight);
		}

		g_destPitch = (systemColorDepth >> 3) * g_destWidth;
		filterPix   = (u8 *)calloc(1, (systemColorDepth >> 3) * g_destWidth * g_destHeight);
	}
	else
#endif
	{
		g_destPitch = g_surface->pitch;
	}

}

#define MOD_KEYS    (KMOD_CTRL|KMOD_SHIFT|KMOD_ALT|KMOD_META)
#define MOD_NOCTRL  (KMOD_SHIFT|KMOD_ALT|KMOD_META)
#define MOD_NOALT   (KMOD_CTRL|KMOD_SHIFT|KMOD_META)
#define MOD_NOSHIFT (KMOD_CTRL|KMOD_ALT|KMOD_META)


/*
 * 04.02.2008 (xKiv): factored out from sdlPollEvents
 *
 */
void change_rewind(int howmuch)
{
	if(	emulating && emulator.emuReadMemState && rewindMemory
			&&	rewindCount
	) {
		rewindPos = (rewindPos + rewindCount + howmuch) % rewindCount;
		emulator.emuReadMemState(
				&rewindMemory[REWIND_SIZE*rewindPos],
				REWIND_SIZE
		);
		rewindCounter = 0;
		{
			char rewindMsgBuffer[50];
			snprintf(rewindMsgBuffer, 50, "Rewind to %1d [%d]", rewindPos+1, rewindSerials[rewindPos]);
			rewindMsgBuffer[49]	= 0;
			systemConsoleMessage(rewindMsgBuffer);
		}
	}
}

/*
 * handle the F* keys (for savestates)
 * given the slot number and state of the SHIFT modifier, save or restore
 * (in savemode 3, saveslot is stored in saveSlotPosition and num means:
 *  4 .. F5: decrease slot number (down to 0)
 *  5 .. F6: increase slot number (up to 7, because 8 and 9 are reserved for backups)
 *  6 .. F7: save state
 *  7 .. F8: load state
 *  (these *should* be configurable)
 *  other keys are ignored
 * )
 */
static void sdlHandleSavestateKey(int num, int shifted)
{
	int action	= -1;
	// 0: load
	// 1: save
	int backuping	= 1; // controls whether we are doing savestate backups

	if ( sdlSaveKeysSwitch == 2 )
	{
		// ignore "shifted"
		switch (num)
		{
		// nb.: saveSlotPosition is base 0, but to the user, we show base 1 indexes (F## numbers)!
		case 4:
			if (saveSlotPosition > 0)
			{
				saveSlotPosition--;
				SLOG( "Changed savestate slot to %d.\n", saveSlotPosition + 1);
			} else
				SLOG( "Can't decrease slotnumber below 1.\n");
			return; // handled
		case 5:
			if (saveSlotPosition < 7)
			{
				saveSlotPosition++;
				SLOG( "Changed savestate slot to %d.\n", saveSlotPosition + 1);
			} else
				SLOG( "Can't increase slotnumber above 8.\n");
			return; // handled
		case 6:
			action	= 1; // save
			break;
		case 7:
			action	= 0; // load
			break;
		default:
			// explicitly ignore
			return; // handled
		}
	}

	if (sdlSaveKeysSwitch == 0 ) /* "classic" VBA: shifted is save */
	{
		if (shifted)
			action	= 1; // save
		else	action	= 0; // load
		saveSlotPosition	= num;
	}
	if (sdlSaveKeysSwitch == 1 ) /* "xKiv" VBA: shifted is load */
	{
		if (!shifted)
			action	= 1; // save
		else	action	= 0; // load
		saveSlotPosition	= num;
	}

	if (action < 0 || action > 1)
	{
		SLOG(  "sdlHandleSavestateKey(%d,%d), mode %d: unexpected action %d.\n",
				num,
				shifted,
				sdlSaveKeysSwitch,
				action
		);
	}

	if (action)
	{        /* save */
		if (backuping)
		{
			sdlWriteState(-1); // save to a special slot
			sdlWriteBackupStateExchange(-1, saveSlotPosition, SLOT_POS_SAVE_BACKUP); // F10
		} else {
			sdlWriteState(saveSlotPosition);
		}
	} else { /* load */
		if (backuping)
		{
			/* first back up where we are now */
			sdlWriteState(SLOT_POS_LOAD_BACKUP); // F9
		}
		sdlReadState(saveSlotPosition);
	}

} // sdlHandleSavestateKey

void sdlPollEvents()
{
	SDL_Event event;
	while(SDL_PollEvent(&event)) {
		switch(event.type) {
		case SDL_QUIT:
			emulating = 0;
			sdlWriteBattery();
			break;
#ifndef _QNXNTO__
		case SDL_VIDEORESIZE:
			if (ignore_first_resize_event)
			{
				ignore_first_resize_event	= 0;
				break;
			}
#endif
#ifndef NO_OGL
			if (g_openGL)
			{
				SDL_SetVideoMode(event.resize.w, event.resize.h, 0,
						SDL_OPENGL | SDL_RESIZABLE |
						(fullscreen ? SDL_FULLSCREEN : 0));
				sdlOpenGLInit(event.resize.w, event.resize.h);
			}
			break;
#endif
		case SDL_ACTIVEEVENT:
			if(pauseWhenInactive && (event.active.state & SDL_APPINPUTFOCUS)) {
				active = event.active.gain;
				if(active) {
					if(!paused) {
						if(emulating)
							soundResume();
					}
				} else {
					wasPaused = true;
					if(pauseWhenInactive) {
						if(emulating)
							soundPause();
					}

					memset(delta,255,sizeof(delta));
				}
			}
			break;
#ifndef __QNXNTO__
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
			if(fullscreen) {
				SDL_ShowCursor(SDL_ENABLE);
				mouseCounter = 120;
			}
			break;
#endif
		case SDL_JOYHATMOTION:
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
		case SDL_JOYAXISMOTION:
		case SDL_SYSWMEVENT:
		case SDL_KEYDOWN:
			inputProcessSDLEvent(event);
			break;

		case SDL_KEYUP:
			switch(event.key.keysym.sym) {
			case SDLK_r:
				if(!(event.key.keysym.mod & MOD_NOCTRL) &&
						(event.key.keysym.mod & KMOD_CTRL)) {
					if(emulating) {
						emulator.emuReset();

						systemScreenMessage("Reset");
					}
				}
				break;
			case SDLK_b:
				if(!(event.key.keysym.mod & MOD_NOCTRL) &&
						(event.key.keysym.mod & KMOD_CTRL))
					change_rewind(-1);
				break;
			case SDLK_v:
				if(!(event.key.keysym.mod & MOD_NOCTRL) &&
						(event.key.keysym.mod & KMOD_CTRL))
					change_rewind(+1);
				break;
			case SDLK_h:
				if(!(event.key.keysym.mod & MOD_NOCTRL) &&
						(event.key.keysym.mod & KMOD_CTRL))
					change_rewind(0);
				break;
			case SDLK_j:
				if(!(event.key.keysym.mod & MOD_NOCTRL) &&
						(event.key.keysym.mod & KMOD_CTRL))
					change_rewind( (rewindTopPos - rewindPos) * ((rewindTopPos>rewindPos) ? +1:-1) );
				break;
			case SDLK_PLUS:
				//   if(!(event.key.keysym.mod & MOD_NOCTRL) &&
				//      (event.key.keysym.mod & KMOD_CTRL)
				//	) {
				if (sdlSoundToggledOff) { // was off
					// restore saved state
					soundSetEnable( sdlSoundToggledOff );
					sdlSoundToggledOff = 0;
					systemConsoleMessage("Sound ON");
				}
				else
				{ // was on
					sdlSoundToggledOff = soundGetEnable();
					soundSetEnable( 0 );
					systemConsoleMessage("Sound OFF");
					if (!sdlSoundToggledOff) {
						sdlSoundToggledOff = 0x30f;
					}
				}
				//	}
				break;
				/*
  case SDLK_KP_DIVIDE:
	sdlChangeVolume(-0.1);
	break;
  case SDLK_KP_MULTIPLY:
	sdlChangeVolume(0.1);
	break;
  case SDLK_KP_MINUS:
	if (gb_effects_config.stereo > 0.0) {
	  gb_effects_config.stereo = 0.0;
	  if (gb_effects_config.echo == 0.0 && !gb_effects_config.surround) {
		gb_effects_config.enabled = 0;
	  }
	  systemScreenMessage("Stereo off");
	} else {
	  gb_effects_config.stereo = SDL_SOUND_STEREO;
	  gb_effects_config.enabled = true;
	  systemScreenMessage("Stereo on");
	}
	break;
  case SDLK_KP_PLUS:
	if (gb_effects_config.echo > 0.0) {
	  gb_effects_config.echo = 0.0;
	  if (gb_effects_config.stereo == 0.0 && !gb_effects_config.surround) {
		gb_effects_config.enabled = false;
	  }
	  systemScreenMessage("Echo off");
	} else {
	  gb_effects_config.echo = SDL_SOUND_ECHO;
	  gb_effects_config.enabled = true;
	  systemScreenMessage("Echo on");
	}
	break;
  case SDLK_KP_ENTER:
	if (gb_effects_config.surround) {
	  gb_effects_config.surround = false;
	  if (gb_effects_config.stereo == 0.0 && gb_effects_config.echo == 0.0) {
		gb_effects_config.enabled = false;
	  }
	  systemScreenMessage("Surround off");
	} else {
	  gb_effects_config.surround =true;
	  gb_effects_config.enabled = true;
	  systemScreenMessage("Surround on");
	}
	break;
				 */
			case SDLK_p:
				if(!(event.key.keysym.mod & MOD_NOCTRL) &&
						(event.key.keysym.mod & KMOD_CTRL)) {
					paused = !paused;
					SDL_PauseAudio(paused);
					if(paused)
						wasPaused = true;
					systemConsoleMessage(paused?"Pause on":"Pause off");
				}
				break;
			case SDLK_ESCAPE:
				emulating = 0;
				break;
			case SDLK_f:
				break;
			case SDLK_g:
				if(!(event.key.keysym.mod & MOD_NOCTRL) &&
						(event.key.keysym.mod & KMOD_CTRL)) {
					filterFunction = 0;
					while (!filterFunction)
					{
						filter = (Filter)((filter + 1) % kInvalidFilter);
						filterFunction = initFilter(filter, systemColorDepth, g_srcWidth);
					}
					if (getFilterEnlargeFactor(filter) != filter_enlarge)
						sdlInitVideo();
					systemScreenMessage(getFilterName(filter));
				  }
				break;
			case SDLK_F11:
#ifndef __QNXNTO__
				if(dbgMain != debuggerMain) {
					if(armState) {
						armNextPC -= 4;
						reg[15].I -= 4;
					} else {
						armNextPC -= 2;
						reg[15].I -= 2;
					}
				}
				debugger = true;
#endif
				break;
			case SDLK_F1:
				cheatsEnabled = !cheatsEnabled;

				if (cheatsEnabled)
				{
					// Create dialog to ask for .cht file
					cheatsEnabled = showCheatSelectionDialog();
					if (!cheatsEnabled)
					{
						systemConsoleMessage("Failed loading CHEAT code");
					}
				}
				systemConsoleMessage(cheatsEnabled?"Cheats on":"Cheats off");
				break;
			case SDLK_F2:
				speedup = (speedup == true) ? false : true;
				systemConsoleMessage(speedup==true?"Fast FWD":"Normal");
				break;
			case SDLK_F3:
				showSpeed ^= 0x8000;
				systemConsoleMessage((showSpeed&0x8000)?"Show FPS":"Hide FPS");
				break;
			case SDLK_F4:
				fullscreen = !fullscreen;
				sdlInitVideo();
				break;
			case SDLK_F5:
			case SDLK_F6:
			case SDLK_F7:
			case SDLK_F8:
				if(!(event.key.keysym.mod & MOD_NOSHIFT) &&
						(event.key.keysym.mod & KMOD_SHIFT)) {
					sdlHandleSavestateKey( event.key.keysym.sym - SDLK_F1, 1); // with SHIFT
				} else if(!(event.key.keysym.mod & MOD_KEYS)) {
					sdlHandleSavestateKey( event.key.keysym.sym - SDLK_F1, 0); // without SHIFT
				}
				break;
				/* backups - only load */
			case SDLK_F9:
				/* F9 is "load backup" - saved state from *just before* the last restore */
				if ( ! (event.key.keysym.mod & MOD_NOSHIFT) ) /* must work with or without shift, but only without other modifiers*/
				{
					sdlReadState(SLOT_POS_LOAD_BACKUP);
				}
				break;
			case SDLK_F10:
				/* F10 is "save backup" - what was in the last overwritten savestate before we overwrote it*/
				if ( ! (event.key.keysym.mod & MOD_NOSHIFT) ) /* must work with or without shift, but only without other modifiers*/
				{
					sdlReadState(SLOT_POS_SAVE_BACKUP);
				}
				break;
			case SDLK_1:
			case SDLK_2:
			case SDLK_3:
			case SDLK_4:
				if(!(event.key.keysym.mod & MOD_NOALT) &&
						(event.key.keysym.mod & KMOD_ALT)) {
					const char *disableMessages[4] =
					{ "autofire A disabled",
							"autofire B disabled",
							"autofire R disabled",
							"autofire L disabled"};
					const char *enableMessages[4] =
					{ "autofire A",
							"autofire B",
							"autofire R",
							"autofire L"};

					EKey k = KEY_BUTTON_A;
					if (event.key.keysym.sym == SDLK_1)
						k = KEY_BUTTON_A;
					else if (event.key.keysym.sym == SDLK_2)
						k = KEY_BUTTON_B;
					else if (event.key.keysym.sym == SDLK_3)
						k = KEY_BUTTON_R;
					else if (event.key.keysym.sym == SDLK_4)
						k = KEY_BUTTON_L;

					if(inputToggleAutoFire(k)) {
						systemScreenMessage(enableMessages[event.key.keysym.sym - SDLK_1]);
					} else {
						systemScreenMessage(disableMessages[event.key.keysym.sym - SDLK_1]);
					}
				} else if(!(event.key.keysym.mod & MOD_NOCTRL) &&
						(event.key.keysym.mod & KMOD_CTRL)) {
					int mask = 0x0100 << (event.key.keysym.sym - SDLK_1);
					layerSettings ^= mask;
					layerEnable = READ_REG(REG_DISPCNT) & layerSettings;
					CPUUpdateRenderBuffers(false);
				}
				break;
			case SDLK_5:
			case SDLK_6:
			case SDLK_7:
			case SDLK_8:
				if(!(event.key.keysym.mod & MOD_NOCTRL) &&
						(event.key.keysym.mod & KMOD_CTRL)) {
					int mask = 0x0100 << (event.key.keysym.sym - SDLK_1);
					layerSettings ^= mask;
					layerEnable = READ_REG(REG_DISPCNT) & layerSettings;
				}
				break;

			case SDLK_0:
				SLOG("ROM selector ...");
				AutoLoadRom();
				break;

			case SDLK_n:
				if(!(event.key.keysym.mod & MOD_NOCTRL) &&
						(event.key.keysym.mod & KMOD_CTRL)) {
					if(paused)
						paused = false;
					pauseNextFrame = true;
				}
				break;
			default:
				break;
			}
			inputProcessSDLEvent(event);
			break;
		}
	}
}

#if WITH_LIRC
void lircCheckInput(void)
{
	if(LIRCEnabled) {
		//setup a poll (poll.h)
		struct pollfd pollLIRC;
		//values fd is the pointer gotten from lircinit and events is what way
		pollLIRC.fd = LIRCfd;
		pollLIRC.events = POLLIN;
		//run the poll
		if( poll( &pollLIRC, 1, 0 ) > 0 ) {
			//poll retrieved something
			char *CodeLIRC;
			char *CmdLIRC;
			int ret; //dunno???
			if( lirc_nextcode(&CodeLIRC) == 0 && CodeLIRC != NULL ) {
				//retrieve the commands
				while( ( ret = lirc_code2char( LIRCConfigInfo, CodeLIRC, &CmdLIRC ) ) == 0 && CmdLIRC != NULL ) {
					//change the text to uppercase
					char *CmdLIRC_Pointer = CmdLIRC;
					while(*CmdLIRC_Pointer != '\0') {
						*CmdLIRC_Pointer = toupper(*CmdLIRC_Pointer);
						CmdLIRC_Pointer++;
					}

					if( strcmp( CmdLIRC, "QUIT" ) == 0 ) {
						emulating = 0;
					} else if( strcmp( CmdLIRC, "PAUSE" ) == 0 ) {
						paused = !paused;
						SDL_PauseAudio(paused);
						if(paused) wasPaused = true;
						systemConsoleMessage( paused?"Pause on":"Pause off" );
						systemScreenMessage( paused?"Pause on":"Pause off" );
					} else if( strcmp( CmdLIRC, "RESET" ) == 0 ) {
						if(emulating) {
							emulator.emuReset();
							systemScreenMessage("Reset");
						}
					} else if( strcmp( CmdLIRC, "MUTE" ) == 0 ) {
						if (sdlSoundToggledOff) { // was off
							// restore saved state
							soundSetEnable( sdlSoundToggledOff );
							sdlSoundToggledOff = 0;
							systemConsoleMessage("Sound toggled on");
						} else { // was on
							sdlSoundToggledOff = soundGetEnable();
							soundSetEnable( 0 );
							systemConsoleMessage("Sound toggled off");
							if (!sdlSoundToggledOff) {
								sdlSoundToggledOff = 0x3ff;
							}
						}
					} else if( strcmp( CmdLIRC, "VOLUP" ) == 0 ) {
						sdlChangeVolume(0.1);
					} else if( strcmp( CmdLIRC, "VOLDOWN" ) == 0 ) {
						sdlChangeVolume(-0.1);
					} else if( strcmp( CmdLIRC, "LOADSTATE" ) == 0 ) {
						sdlReadState(saveSlotPosition);
					} else if( strcmp( CmdLIRC, "SAVESTATE" ) == 0 ) {
						sdlWriteState(saveSlotPosition);
					} else if( strcmp( CmdLIRC, "1" ) == 0 ) {
						saveSlotPosition = 0;
						systemScreenMessage("Selected State 1");
					} else if( strcmp( CmdLIRC, "2" ) == 0 ) {
						saveSlotPosition = 1;
						systemScreenMessage("Selected State 2");
					} else if( strcmp( CmdLIRC, "3" ) == 0 ) {
						saveSlotPosition = 2;
						systemScreenMessage("Selected State 3");
					} else if( strcmp( CmdLIRC, "4" ) == 0 ) {
						saveSlotPosition = 3;
						systemScreenMessage("Selected State 4");
					} else if( strcmp( CmdLIRC, "5" ) == 0 ) {
						saveSlotPosition = 4;
						systemScreenMessage("Selected State 5");
					} else if( strcmp( CmdLIRC, "6" ) == 0 ) {
						saveSlotPosition = 5;
						systemScreenMessage("Selected State 6");
					} else if( strcmp( CmdLIRC, "7" ) == 0 ) {
						saveSlotPosition = 6;
						systemScreenMessage("Selected State 7");
					} else if( strcmp( CmdLIRC, "8" ) == 0 ) {
						saveSlotPosition = 7;
						systemScreenMessage("Selected State 8");
					} else {
						//do nothing
					}
				}
				//we dont need this code nomore
				free(CodeLIRC);
			}
		}
	}
}
#endif

void usage(char *cmd)
{
	printf("%s [option ...] file\n", cmd);
	printf("\
			\n\
			Options:\n\
			-O, --opengl=MODE            Set OpenGL texture filter\n\
			  --no-opengl               0 - Disable OpenGL\n\
			  --opengl-nearest          1 - No filtering\n\
			  --opengl-bilinear         2 - Bilinear filtering\n\
			-F, --fullscreen             Full screen\n\
			-G, --gdb=PROTOCOL           GNU Remote Stub mode:\n\
										tcp      - use TCP at port 55555\n\
										tcp:PORT - use TCP at port PORT\n\
										pipe     - use pipe transport\n\
			-I, --ifb-filter=FILTER      Select interframe blending filter:\n\
			");
	for (int i  = 0; i < (int)kInvalidIFBFilter; i++)
		printf("                                %d - %s\n", i, getIFBFilterName((IFBFilter)i));
	printf("\
			-N, --no-debug               Don't parse debug information\n\
			-S, --flash-size=SIZE        Set the Flash size\n\
			  --flash-64k               0 -  64K Flash\n\
			  --flash-128k              1 - 128K Flash\n\
			-T, --throttle=THROTTLE      Set the desired throttle (5...1000)\n\
			-b, --bios=BIOS              Use given bios file\n\
			-c, --config=FILE            Read the given configuration file\n\
			-d, --debug                  Enter debugger\n\
			-f, --filter=FILTER          Select filter:\n\
			");
				for (int i  = 0; i < (int)kInvalidFilter; i++)
					printf("                                %d - %s\n", i, getFilterName((Filter)i));
				printf("\
			-h, --help                   Print this help\n\
			-i, --patch=PATCH            Apply given patch\n\
			-p, --profile=[HERTZ]        Enable profiling\n\
			-s, --frameskip=FRAMESKIP    Set frame skip (0...9)\n\
			-t, --save-type=TYPE         Set the available save type\n\
			  --save-auto               0 - Automatic (EEPROM, SRAM, FLASH)\n\
			  --save-eeprom             1 - EEPROM\n\
			  --save-sram               2 - SRAM\n\
			  --save-flash              3 - FLASH\n\
			  --save-sensor             4 - EEPROM+Sensor\n\
			  --save-none               5 - NONE\n\
			-v, --verbose=VERBOSE        Set verbose logging (trace.log)\n\
										  1 - SWI\n\
										  2 - Unaligned memory access\n\
										  4 - Illegal memory write\n\
										  8 - Illegal memory read\n\
										 16 - DMA 0\n\
										 32 - DMA 1\n\
										 64 - DMA 2\n\
										128 - DMA 3\n\
										256 - Undefined instruction\n\
										512 - AGBPrint messages\n\
			\n\
			Long options only:\n\
			  --agb-print              Enable AGBPrint support\n\
			  --auto-frameskip         Enable auto frameskipping\n\
			  --no-agb-print           Disable AGBPrint support\n\
			  --no-auto-frameskip      Disable auto frameskipping\n\
			  --no-patch               Do not automatically apply patch\n\
			  --no-pause-when-inactive Don't pause when inactive\n\
			  --no-rtc                 Disable RTC support\n\
			  --no-show-speed          Don't show emulation speed\n\
			  --no-throttle            Disable throttle\n\
			  --pause-when-inactive    Pause when inactive\n\
			  --rtc                    Enable RTC support\n\
			  --show-speed-normal      Show emulation speed\n\
			  --show-speed-detailed    Show detailed speed data\n\
			  --cheat 'CHEAT'          Add a cheat\n\
			");
}

/*
 * 04.02.2008 (xKiv) factored out, reformatted, more usefuler rewinds browsing scheme
 */
void handleRewinds()
{
	int curSavePos; // where we are saving today [1]

	rewindCount++;  // how many rewinds will be stored after this store
	if(rewindCount > REWIND_NUM)
		rewindCount = REWIND_NUM;

	curSavePos	= (rewindTopPos + 1) % rewindCount; // [1] depends on previous

	if(
			emulator.emuWriteMemState
			&&
			emulator.emuWriteMemState(
					&rewindMemory[curSavePos*REWIND_SIZE],
					REWIND_SIZE
			)
	) {
		char rewMsgBuf[100];
		snprintf(rewMsgBuf, 100, "Remembered rewind %1d (of %1d), serial %d.", curSavePos+1, rewindCount, rewindSerial);
		rewMsgBuf[99]	= 0;
		systemConsoleMessage(rewMsgBuf);
		rewindSerials[curSavePos]	= rewindSerial;

		// set up next rewind save
		// - don't clobber the current rewind position, unless it is the original top
		if (rewindPos == rewindTopPos) {
			rewindPos = curSavePos;
		}
		// - new identification and top
		rewindSerial++;
		rewindTopPos = curSavePos;
		// for the rest of the code, rewindTopPos will be where the newest rewind got stored
	}
}

static bool check_mkdir( const char *path, mode_t mode )
{
	struct stat64 st;
	if(stat64(path, &st) == 0)
	{
	    if( (S_ISDIR(st.st_mode)) )
	    {
	    	return true;
	    }
	}

	if ( (mkdir(path, mode) != 0) && (errno != EEXIST) )
	{
		return false;
	}

	return true;
}

int main(int argc, char **argv)
{
	IMAGE_TYPE type;

	SLOG( "GBAEMU version %s [SDL]\n", GBA_VERSION);

	bps_initialize();
	dialog_request_events(0);

	/*
	 * From OS 10.3.1, dialog box require a parent window, so we
	 * need to call SDL_Init to create the main parent window.
	 */
	SLOG("Initialize SDL ...\n");

	int flags = SDL_INIT_VIDEO|SDL_INIT_AUDIO|
			SDL_INIT_TIMER|SDL_INIT_NOPARACHUTE;

	if(SDL_Init(flags)) {
		SLOG("Failed to init SDL: %s", SDL_GetError());
		exit(-1);
	}

	arg0 = argv[0];

	captureDir[0] = 0;
	saveDir[0] = 0;
	batteryDir[0] = 0;

	int op = -1;

	frameSkip  = 1;
	gbBorderOn = 0;

	parseDebug = false;

	gb_effects_config.stereo = 0.0;
	gb_effects_config.echo = 0.0;
	gb_effects_config.surround = false;
	gb_effects_config.enabled = false;

#ifdef __QNXNTO__
	// Get home dir

	if (false == check_mkdir("/accounts/1000/shared/misc/roms",0777) ) return -1;
	if (false == check_mkdir("/accounts/1000/shared/misc/gbaemu", 0777) ) return -1;
	if (false == check_mkdir("/accounts/1000/shared/misc/gbaemu/savegames", 0777) ) return -1;

	const char *vbaPath = "/accounts/1000/shared/misc/gbaemu";
	if (false == check_mkdir("/accounts/1000/shared/misc/roms/gba",0777) ) return -1;
	if (false == check_mkdir(vbaPath,0777) ) return -1;

	homeDir = (char *)vbaPath;
	useBios = true;
	strcpy(biosFileName, SYSROMDIR"gba.bin");

	/*
	 * Always overwrite vbam-over.ini
	 */
	{
		ifstream f1("app/native/vba-over.ini", fstream::binary);
		ofstream f2("/accounts/1000/shared/misc/gbaemu/vbam-over.ini", fstream::trunc|fstream::binary);
		f2 << f1.rdbuf();
	}

	ifstream ifile2("/accounts/1000/shared/misc/gbaemu/vbam.cfg");
	if(!ifile2){
		ifstream f11("app/native/vbam.cfg", fstream::binary);
		ofstream f22("/accounts/1000/shared/misc/gbaemu/vbam.cfg", fstream::trunc|fstream::binary);
		f22 << f11.rdbuf();
	} else {
		ifile2.close();
	}

	ifstream ifilecore("logs/GBA_bb.core", fstream::binary);
	if(ifilecore) {
		ofstream fOut(SYSCONFDIR"/GBA_bb.core", fstream::trunc|fstream::binary);
		fOut << ifilecore.rdbuf();
		ifilecore.close();

		system("rm logs/GBA_bb.core");

		bbDialog *dialog = new (bbDialog);

		if (dialog)
		{
			dialog->showAlert("     GBAEMU CoreDump Detected", "A previous Crash COREDUMP is detected, and the COREDUMP file is transferred to misc/gbaemu/GBA_bb.core!!");
			delete dialog;
		}

	}

	sdlReadPreferences();

	// Create ROM files list
	UpdateRomList();
#endif

	fflush(stdout);
	sdlPrintUsage = 0;


	while((op = getopt_long(argc,
			argv,
			"FNO:T:Y:G:I:D:b:c:df:hi:p::s:t:v:",
			sdlOptions,
			NULL)) != -1) {
		switch(op) {
		case 0:
			// long option already processed by getopt_long
			break;
		case 1000:
			// --cheat
			if (sdlPreparedCheats >= MAX_CHEATS) {
				SLOG( "Warning: cannot add more than %d cheats.\n", MAX_CHEATS);
				break;
			}
			{
				strncpy(sdlPreparedCheatCodes[sdlPreparedCheats++], optarg, MAX_CHEAT_LEN-1);
			}
			break;
		case 1001:
			// --autofire
			autoFireMaxCount = sdlFromDec(optarg);
			if (autoFireMaxCount < 1)
				autoFireMaxCount = 1;
			break;
		case 'b':
			useBios = true;
			if(optarg == NULL) {
				SLOG( "Missing BIOS file name\n");
				exit(-1);
			}
			strcpy(biosFileName, optarg);
			break;
		case 'c':
		{
			if(optarg == NULL) {
				SLOG( "Missing config file name\n");
				exit(-1);
			}
			FILE *f = fopen(optarg, "r");
			if(f == NULL) {
				SLOG( "File not found %s\n", optarg);
				exit(-1);
			}
			sdlReadPreferences(f);
			fclose(f);
		}
		break;
		case 'd':
			debugger = true;
			break;
		case 'h':
			sdlPrintUsage = 1;
			break;
		case 'i':
			if(optarg == NULL) {
				SLOG( "Missing patch name\n");
				exit(-1);
			}
			if (sdl_patch_num >= PATCH_MAX_NUM) {
				SLOG( "Too many patches given at %s (max is %d). Ignoring.\n", optarg, PATCH_MAX_NUM);
			} else {
				sdl_patch_names[sdl_patch_num]	= (char *)malloc(1 + strlen(optarg));
				strcpy(sdl_patch_names[sdl_patch_num], optarg);
				sdl_patch_num++;
			}
			break;
		case 'G':
#ifndef __QNXNTO__
			dbgMain = remoteStubMain;
			dbgSignal = remoteStubSignal;
			dbgOutput = remoteOutput;
			debugger = true;
			debuggerStub = true;
			if(optarg) {
				char *s = optarg;
				if(strncmp(s,"tcp:", 4) == 0) {
					s+=4;
					int port = atoi(s);
					remoteSetProtocol(0);
					remoteSetPort(port);
				} else if(strcmp(s,"tcp") == 0) {
					remoteSetProtocol(0);
				} else if(strcmp(s, "pipe") == 0) {
					remoteSetProtocol(1);
				} else {
					SLOG( "Unknown protocol %s\n", s);
					exit(-1);
				}
			} else {
				remoteSetProtocol(0);
			}
#endif
			break;

		case 'N':
			parseDebug = false;
			break;
		case 'D':
			if(optarg) {
				systemDebug = atoi(optarg);
			} else {
				systemDebug = 1;
			}
			break;
		case 'F':
			fullscreen = 1;
			mouseCounter = 120;
			break;
		case 'f':
			if(optarg) {
				filter = (Filter)atoi(optarg);
			} else {
				filter = kStretch1x;
			}
			break;
		case 'I':
			if(optarg) {
				ifbType = (IFBFilter)atoi(optarg);
			} else {
				ifbType = kIFBNone;
			}
			break;
		case 'p':
#ifdef PROFILING
			if(optarg) {
				cpuEnableProfiling(atoi(optarg));
			} else
				cpuEnableProfiling(100);
#endif
			break;
		case 'S':
			sdlFlashSize = atoi(optarg);
			if(sdlFlashSize < 0 || sdlFlashSize > 1)
				sdlFlashSize = 0;
			break;
		case 's':
			if(optarg) {
				int a = atoi(optarg);
				if(a >= 0 && a <= 9) {
					gbFrameSkip = a;
					frameSkip = a;
				}
			} else {
				frameSkip = 2;
				gbFrameSkip = 0;
			}
			break;
		case 't':
			if(optarg) {
				int a = atoi(optarg);
				if(a < 0 || a > 5)
					a = 0;
				cpuSaveType = a;
			}
			break;
		case 'v':
			if(optarg) {
				systemVerbose = atoi(optarg);
			} else
				systemVerbose = 0;
			break;
		case '?':
			sdlPrintUsage = 1;
			break;
		case 'O':
			if(optarg) {
				g_openGL = atoi(optarg);
				if (g_openGL < 0 || g_openGL > 2)
					g_openGL = 1;
			} else
				g_openGL = 0;
			break;

		}
	}


	if(sdlPrintUsage) {
		usage(argv[0]);
		exit(-1);
	}

	if(rewindTimer) {
		rewindMemory = (char *)malloc(REWIND_NUM*REWIND_SIZE);
		rewindSerials = (int *)calloc(REWIND_NUM, sizeof(int)); // init to zeroes
	}

	if(sdlFlashSize == 0)
		flashSetSize(0x10000);
	else
		flashSetSize(0x20000);

	rtcEnable(sdlRtcEnable ? true : false);
	agbPrintEnable(sdlAgbPrint ? true : false);

	for(int i = 0; i < 24;) {
		systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
		systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
		systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
		systemGbPalette[i++] = 0;
	}

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	if(1) {
#ifdef __QNXNTO__
		const char   *szFile = 0;
		const char  **list = 0;
		int           count = 0;
		string        romfilename;
		bbDialog     *dialog;

		SLOG("Sorted List: %d", sortedRomList.size() );
		list = (const char**)malloc(sortedRomList.size()*sizeof(char*));

		// ROM selection
		if (list)
		{
			for(count = 0; count < sortedRomList.size(); count++)
			{
				romfilename = sortedRomList.at(count);
				list[count] = sortedRomList[count].c_str() + romfilename.find_last_of("/") + 1;
			}

			SLOG("Creating BB dialog...");
			dialog = new bbDialog;

			if (dialog)
			{
				gameIndex = dialog->showPopuplistDialog(list, sortedRomList.size(), "GBAEMU [v" GBA_VERSION "]  ROM Selector");
				delete dialog;
				free(list);

				if (gameIndex >= 0)
				{
					romfilename = sortedRomList.at(gameIndex);
				}
				else
				{
					SLOG("Bad selection index from Popup List Dialog");
					return -1;
				}
			}
			else
			{
				SLOG("Fail creating BB dialog, quiting");
				return -1;
			}
		}
		else
		{
			SLOG("Out of Memory, Fail creating ROM list, quiting!!");
			return -1;
		}

		memset(&g_runningFile_str[0],0,64);
		sprintf(&g_runningFile_str[0], romfilename.c_str());

		SLOG("ROM loading: %d/%d '%s'\n",gameIndex + 1, sortedRomList.size(), romfilename.c_str() );
		strcpy(filename, romfilename.c_str());
		szFile = romfilename.c_str();
		SLOG("%s",szFile);
#else
		char *szFile = argv[optind];
#endif

		SLOG("szFile = %s\n", szFile);

		u32 len = strlen(szFile);
		if (len > SYSMSG_BUFFER_SIZE)
		{
			SLOG("%s :%s: File name too long\n",argv[0],szFile);
			exit(0);
		}

		if (sdlAutoPatch && sdl_patch_num == 0)
		{
			char * tmp;
			// no patch given yet - look for ROMBASENAME.ips
			tmp = (char *)malloc(strlen(filename) + 4 + 1);
			sprintf(tmp, "%s.ips", filename);
			sdl_patch_names[sdl_patch_num] = tmp;
			sdl_patch_num++;

			// no patch given yet - look for ROMBASENAME.ups
			tmp = (char *)malloc(strlen(filename) + 4 + 1);
			sprintf(tmp, "%s.ups", filename);
			sdl_patch_names[sdl_patch_num] = tmp;
			sdl_patch_num++;

			// no patch given yet - look for ROMBASENAME.ppf
			tmp = (char *)malloc(strlen(filename) + 4 + 1);
			sprintf(tmp, "%s.ppf", filename);
			sdl_patch_names[sdl_patch_num] = tmp;
			sdl_patch_num++;
		}

		SLOG("soundInit..\n");
		soundInit();

		bool failed = false;

		if(strlen(szFile) <= 4)
		{
			SLOG(" ROM file is invalid ...\n");
			exit(-1);
		}

		type = utilFindType(szFile);

		SLOG("'%s' type = %d\n",szFile, type);

		if(type == IMAGE_UNKNOWN) {
			systemMessage(MSG_UNKNOWN_CARTRIDGE_TYPE, "Unknown file type %s", szFile);
			exit(-1);
		}
		cartridgeType = (int)type;

		if(type == IMAGE_GB)
		{
			SLOG("GB image '%s'\n",szFile);
			failed = !gbLoadRom(szFile);
			if(!failed) {
				gbGetHardwareType();

				// used for the handling of the gb Boot Rom
				if (gbHardware & 5)
					gbCPUInit(gbBiosFileName, useBios);

				cartridgeType = IMAGE_GB;
				emulator = GBSystem;

				int size = gbRomSize, patchnum;
				for (patchnum = 0; patchnum < sdl_patch_num; patchnum++) {
					fprintf(stdout, "Trying patch %s%s\n", sdl_patch_names[patchnum],
							applyPatch(sdl_patch_names[patchnum], &gbRom, &size) ? " [success]" : "");
				}
				if(size != gbRomSize) {
					gbUpdateSizes();
					gbReset();
				}
				gbReset();
				gbSoundSetDeclicking(systemSoundDeclicking != 0);
			}
		}
		else if(type == IMAGE_GBA)
		{
			SLOG("GBA image '%s'\n",szFile);
			int size = CPULoadRom(szFile);
			failed = (size == 0);
			if(!failed) {
				sdlApplyPerImagePreferences();

				doMirroring(mirroringEnable);

				cartridgeType = 0;
				emulator = GBASystem;

				SLOG("CPUInit: '%s' %d", biosFileName, useBios);

				useBios = false;
				CPUInit(biosFileName, useBios);
				int patchnum;
				for (patchnum = 0; patchnum < sdl_patch_num; patchnum++) {
					SLOG( "Trying patch %s%s\n", sdl_patch_names[patchnum],
							applyPatch(sdl_patch_names[patchnum], &rom, &size) ? " [success]" : "");
				}
				CPUReset();
			}
			else
			{
				bbDialog     outofmemDialog;

				outofmemDialog.showNotification("ROM loading failure, possible out of memory.\nPlease try again, Quiting now!!");

				exit(-1);
			}
		}

		if(failed)
		{
			systemMessage(MSG_UNKNOWN_CARTRIDGE_TYPE, "Failed to load file %s", szFile);
		}
	}

	sdlReadBattery();

	if(debuggerStub)
		remoteInit();

	if(SDL_InitSubSystem(SDL_INIT_JOYSTICK)) {
		SLOG("Failed to init joystick support: %s", SDL_GetError());
	}

#if WITH_LIRC
	StartLirc();
#endif
	inputInitJoysticks();

	initGbFrameSize();

	SLOG("initialize video mode ...");
	sdlReadDesktopVideoMode();
	sdlInitVideo();


	SLOG("initialize filter ...");
	filterFunction = initFilter(filter, systemColorDepth, g_srcWidth);
	if (!filterFunction)
	{
		bbDialog dialog;

		dialog.showNotification("Unable to initialize Filter, quiting!!");

		return -1;
	}

	if(systemColorDepth == 15)
		systemColorDepth = 16;

	if(systemColorDepth != 16 && systemColorDepth != 24 && systemColorDepth != 32) {
		SLOG("Unsupported color depth '%d'.\nOnly 16, 24 and 32 bit color depths are supported\n", systemColorDepth);
		exit(-1);
	}

	SLOG("Color depth: %d  update color maps ...", systemColorDepth);


	utilUpdateSystemColorMaps(false);

	if(delta == NULL) {
		delta = (u8*)malloc(322*242*4);
		memset(delta, 255, 322*242*4);
	}

	ifbFunction = initIFBFilter(ifbType, systemColorDepth);

	emulating = 1;

	systemFrameInit();

	if (type == IMAGE_GBA)
	{
		if(systemSoundQuality != 0)
		{
			SLOG("SOUND set sampling rate (%dHz)", 44100 / systemSoundQuality);
			soundSetSampleRate(44100 / systemSoundQuality);
		}

		if (systemSoundRes != 0)
		{
			SLOG("SOUND set channels (0x%X)", systemSoundRes);
			soundSetEnable(systemSoundRes);
		}
	}


	if( InitLink() )
	{
		SLOG("Fail GBA Link initialization");
	}

	SDL_WM_SetCaption("VBA-M", NULL);

	SLOG("GO emu loop!\n");

	while(emulating) {
		if(!paused && active) {
			// if(debugger && emulator.emuHasDebugger)
			//    dbgMain();
			//  else {
			emulator.emuMain(emulator.emuCount);

			if(rewindSaveNeeded && rewindMemory && emulator.emuWriteMemState)
			{
				handleRewinds();
			}

			rewindSaveNeeded = false;

			//}
		} else {
			SDL_Delay(500);
		}
		sdlPollEvents();
#if WITH_LIRC
		lircCheckInput();
#endif
		/*
if(mouseCounter) {
  mouseCounter--;
  if(mouseCounter == 0)
	SDL_ShowCursor(SDL_DISABLE);
}
		 */
	}

	emulating = 0;
	SLOG("Shutting down\n");
	remoteCleanUp();
	soundShutdown();

	if(gbRom != NULL || rom != NULL) {
		sdlWriteBattery();
		emulator.emuCleanUp();
	}

	if(delta) {
		free(delta);
		delta = NULL;
	}

	if(filterPix) {
		free(filterPix);
		filterPix = NULL;
	}

	for (int i = 0; i < sdl_patch_num; i++) {
		free(sdl_patch_names[i]);
	}

#if WITH_LIRC
	StopLirc();
#endif

	SDL_Quit();
	if (g_flogfile) fclose(g_flogfile);

	return 0;
}




#ifdef __WIN32__
extern "C" {
int WinMain()
{
	return(main(__argc, __argv));
}
}
#endif

#if 0
void systemMessage(int num, const char *msg, ...)
{
	char buffer[SYSMSG_BUFFER_SIZE*2];
	va_list valist;

	va_start(valist, msg);
	vsprintf(buffer, msg, valist);

	SLOG( "%s\n", buffer);
	va_end(valist);
}
#endif

void drawScreenMessage(u8 *screen, int pitch, int x, int y, unsigned int duration)
{
	if(screenMessage) {
		if(cartridgeType == 1 && gbBorderOn) {
			gbSgbRenderBorder();
		}
		if(((systemGetClock() - screenMessageTime) < duration) &&
				!disableStatusMessages) {
			drawText(screen, pitch, x, y,
					screenMessageBuffer, false);
		} else {
			screenMessage = false;
		}
	}
}

void drawSpeed(u8 *screen, int pitch, int x, int y)
{
	char buffer[50];

	snprintf(buffer, sizeof(buffer), "%3.3f fps", curFps);

	drawText(screen, pitch, x, y, buffer, showSpeedTransparent);
}

#define ALIGN_DATA(_a_, _b_) (((_a_) + ((_b_) - 1)) & (~((_b_) - 1)))

void systemDrawScreen()
{
	u8 *srcbuf = (u8 *)pix;
	u8 *dstbuf = (u8 *)g_surface->pixels;

#ifndef NO_OGL
	if (g_openGL)
		dstbuf = filterPix;
#endif

	if (NULL == dstbuf)
	{
		SLOG("NULL dst buffer");
		return;
	}

	/*
	if (ifbFunction)
	    ifbFunction(pix + g_srcPitch, g_srcPitch, g_srcWidth, g_srcHeight);
	 */
	if (!g_openGL)
	{
		filterFunction(
				srcbuf,                 // source buffer
				g_srcPitch, 			// source buffer pitch
				delta, 					// delta buffer
				dstbuf,					// destination buffer
				g_destPitch, 			// destination buffer pitch
				g_srcWidth, 			// source width
				g_srcHeight);			// source height
	}
	else
	{
		dstbuf = srcbuf;
	}
	//  drawScreenMessage(screen, destPitch, 10, g_destHeight - 20, 3000);

	if (showSpeed & 0x8000)
	    drawSpeed(dstbuf, g_destPitch, 5, 5);

#ifndef NO_OGL
	if (g_openGL) {
#ifdef __PLAYBOOK__
		updateSurface(g_destWidth, g_destHeight, dstbuf);
#else
		glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3i(0, 0, 0);
		glTexCoord2f(g_destWidth / (GLfloat) g_textureSize, 0.0f);
		glVertex3i(1, 0, 0);
		glTexCoord2f(0.0f, g_destHeight / (GLfloat) g_textureSize);
		glVertex3i(0, 1, 0);
		glTexCoord2f(g_destWidth / (GLfloat) g_textureSize,
				g_destHeight / (GLfloat) g_textureSize);
		glVertex3i(1, 1, 0);
		glEnd();
#endif //__PLAYBOOK__
		SDL_GL_SwapBuffers();
	} else {
		SDL_Flip(g_surface);
	}
#else
	// SDL_UnlockSurface(g_surface);
	//SDL_Delay(1);
	SDL_Flip(g_surface);
#endif

}

void systemSetTitle(const char *title)
{
	SDL_WM_SetCaption(title, NULL);
}

void systemShowSpeed(int speed)
{
	systemSpeed = speed;

	showRenderedFrames = renderedFrames;
	renderedFrames = 0;

	if(!fullscreen && showSpeed) {
		char buffer[80];
		if(showSpeed == 1)
			sprintf(buffer, "GBA - %d%%", systemSpeed);
		else
			sprintf(buffer, "GBA - %d%%(%d, %d fps)", systemSpeed,
					systemFrameSkip,
					showRenderedFrames);

		systemSetTitle(buffer);
	}
}

static int debugFrameCount = 0;
static int debugTotalFrameCount = 0;
static u32 debugFirstFrameTime = 0;
static u32 debugElaspedTime  = 0;
static int debugDriftTime = 0;
static int debugSkip = 0;

void systemFrameInit(void)
{
	autoFrameSkipLastTime = throttleLastTime = systemGetClock();
	debugFrameCount       = 0;
	debugTotalFrameCount  = 0;
	debugFirstFrameTime   = autoFrameSkipLastTime;
	debugElaspedTime      = autoFrameSkipLastTime;
	debugDriftTime        = 16667; //16.667ms drift tolerance
	debugSkip             = 0;

	frameskipadjust       = 1;
	systemFrameSkip       = 0;
	renderedFrames        = 0;

}

void systemFrame()
{
	u32 time = systemGetClock();
	u32 totalElasped   = time - debugFirstFrameTime;
	u32 curElaspedTime = time - debugElaspedTime;
	int curSkip;

	++debugFrameCount;
	++debugTotalFrameCount;
	systemFrameSkip = 0;
#if 1
	// 1 sec elasped
	if ( curElaspedTime >= 1000)
	{
		debugDriftTime = ( (debugTotalFrameCount * 16667)-(totalElasped * 1000) );
		curFps         = (float)debugFrameCount*1000.0/(float)curElaspedTime;
		debugFrameCount   = 0;
		debugElaspedTime += curElaspedTime;

		if(autoFrameSkip)
		{
#if 0
			curSkip = (int)((61.00 / (60.0 - curFps)) * 0.9);
			if (curSkip <= 0)       curSkip = 0;
			else if (curSkip < 2)   curSkip = 2;
			else if (curSkip > 15)  curSkip = 15;

			if (curSkip == 0)        debugSkip = 0;
			else if (curSkip < debugSkip) debugSkip-=2;
			else                     debugSkip = curSkip;
#else
			systemRenderFps = (int)(curFps - 1.0);
#endif
		}
		SLOG("Elasped:%dms, fps:%3.3f, drift:%3.2fms, skip every %d frame", curElaspedTime, curFps, (float)debugDriftTime/1000.0f, debugSkip);

//		SLOG("%d %d | %d %d | %d %d | %d %d | %d %d %d %d | %d %d",
//				__lsl_imm, __lsl_imm_nc,
//				__lsl_reg, __lsl_reg_nc,
//				__lsr_imm, __lsr_imm_nc,
//				__lsr_reg, __lsr_reg_nc,
//				__asr_imm, __asr_reg, __ror_imm, __ror_reg,
//				__imm, __imm_nc);
	}
#endif
#if 1
	if(autoFrameSkip)
	{
		++frameskipadjust;
		if (debugSkip && (frameskipadjust >= debugSkip))
		{
			frameskipadjust = 0;
			systemFrameSkip = 1;
		}
	}
#endif
}

#define MAX_SKIP_FRAME 1

void system10Frames(int rate)
{
	u32 time = systemGetClock();
#if 0
	if(!wasPaused && autoFrameSkip) {
		u32 diff = time - autoFrameSkipLastTime;
		int speed = 100;

		if(diff)
			speed = (1000000/rate)/diff;

		if(speed >= 98) {
			frameskipadjust++;

			if(frameskipadjust >= 2) {
				frameskipadjust=0;
				if(systemFrameSkip > 0)
					systemFrameSkip--;
			}
		} else {
			if(speed  < 80)
				frameskipadjust -= (90 - speed)/2;
			else if(systemFrameSkip < MAX_SKIP_FRAME)
				frameskipadjust--;

			if(frameskipadjust <= -2) {
				frameskipadjust += 2;
				if(systemFrameSkip < MAX_SKIP_FRAME)
					systemFrameSkip++;
			}
		}
	}
#endif
	if(rewindMemory) {
		if(++rewindCounter >= rewindTimer) {
			rewindSaveNeeded = true;
			rewindCounter = 0;
		}
	}

	if(systemSaveUpdateCounter) {
		if(--systemSaveUpdateCounter <= SYSTEM_SAVE_NOT_UPDATED) {
			sdlWriteBattery();
			systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
		}
	}

	wasPaused = false;
	autoFrameSkipLastTime = time;
}

void systemScreenCapture(int a)
{
	char buffer[2048];

	if(captureFormat) {
		if(captureDir[0])
			sprintf(buffer, "%s/%s%02d.bmp", captureDir, sdlGetFilename(filename), a);
		else if (homeDir)
			sprintf(buffer, "%s/%s/%s%02d.bmp", homeDir, DOT_DIR, sdlGetFilename(filename), a);
		else
			sprintf(buffer, "%s%02d.bmp", filename, a);

		emulator.emuWriteBMP(buffer);
	} else {
		if(captureDir[0])
			sprintf(buffer, "%s/%s%02d.png", captureDir, sdlGetFilename(filename), a);
		else if (homeDir)
			sprintf(buffer, "%s/%s/%s%02d.png", homeDir, DOT_DIR, sdlGetFilename(filename), a);
		else
			sprintf(buffer, "%s%02d.png", filename, a);
		emulator.emuWritePNG(buffer);
	}

	systemScreenMessage("Screen capture");
}

u32 systemGetClock()
{
	return SDL_GetTicks();
}

void systemGbPrint(u8 *data,int len,int pages,int feed,int palette, int contrast)
{
}

void systemConsoleMessage(const char *msg)
{
	bbDialog dialog;

	dialog.showNotification(msg);
}

void systemScreenMessage(const char *msg)
{

	screenMessage = true;
	screenMessageTime = systemGetClock();
	if(strlen(msg) > 20) {
		strncpy(screenMessageBuffer, msg, 20);
		screenMessageBuffer[20] = 0;
	} else
		strcpy(screenMessageBuffer, msg);

	systemConsoleMessage(msg);
}

bool systemCanChangeSoundQuality()
{
	return false;
}

bool systemPauseOnFrame()
{
	if(pauseNextFrame) {
		paused = true;
		pauseNextFrame = false;
		return true;
	}
	return false;
}

void systemGbBorderOn()
{
	g_srcWidth         = gbWidth;
	g_srcHeight        = gbHeight;
	gbBorderLineSkip   = gbWidth;
	gbBorderColumnSkip = (gbWidth  - GB_ACTUAL_WIDTH)/2;
	gbBorderRowSkip    = (gbHeight - GB_ACTUAL_HEIGHT)/2;

	sdlInitVideo();

	filterFunction = initFilter(filter, systemColorDepth, g_srcWidth);
}

bool systemReadJoypads()
{
	return true;
}

u32 systemReadJoypad(int which)
{
	return inputReadJoypad(which);
}

void systemUpdateMotionSensor()
{
	inputUpdateMotionSensor();
}

int systemGetSensorX()
{
	return inputGetSensorX();
}

int systemGetSensorY()
{
	return inputGetSensorY();
}

static SoundDriver *sdlSndDriver = NULL;

SoundDriver * systemSoundInit()
{
	soundShutdown();

	sdlSndDriver = new SoundSDL();
	return sdlSndDriver;
}

void systemOnSoundShutdown()
{
	if (sdlSndDriver)
		delete sdlSndDriver;
	sdlSndDriver = NULL;
}

void systemOnWriteDataToSoundBuffer(const u16 * finalWave, int length)
{
	if (sdlSndDriver)
	{
		sdlSndDriver->write((uint16 *)finalWave, length);
	}
}

void log(const char *defaultMsg, ...)
{
	static FILE *out = NULL;

	if(out == NULL) {
		out = fopen("trace.log","w");
	}

	va_list valist;

	va_start(valist, defaultMsg);
	vfprintf(out, defaultMsg, valist);
	va_end(valist);
}
