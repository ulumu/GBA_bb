// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2008 VBA-M development team

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

#ifndef __VBA_SOUND_SDL_H__
#define __VBA_SOUND_SDL_H__

#include "SoundDriver.h"
#include "RingBuffer.h"

#include <SDL.h>

extern int   g_logtofile;
extern FILE *g_flogfile;

#if __DEBUG__
#include <sys/slog.h>
#include <sys/slogcodes.h>

#define SLOG(fmt, ...) \
	if (g_logtofile)    \
	{    fprintf(g_flogfile, "[GBA-LOG][%s:%d]:"fmt"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); fflush(g_flogfile); } \
    else               \
        /*slogf(_SLOG_SETCODE(_SLOGC_TEST+328, 0), _SLOG_DEBUG1, "[GBA-LOG][%s:%d]:"fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)*/ \
        printf("[GBA-LOG][%s:%d]:"fmt"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); fflush(stdout)
#else
#define SLOG(fmt, ...)  \
		if (g_logtofile)    \
		{    fprintf(g_flogfile, "[GBA-LOG][%s:%d]:"fmt"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); fflush(g_flogfile);  \
		}
#endif

class SoundSDL: public SoundDriver
{
public:
	SoundSDL();
	virtual ~SoundSDL();

	virtual bool init(long sampleRate);
	virtual void pause();
	virtual void reset();
	virtual void resume();
	virtual void write(u16 * finalWave, int length);

private:
	RingBuffer<u16> _rbuf;

	SDL_cond  * _cond;
	SDL_mutex * _mutex;

	bool _initialized;

	// Defines what delay in seconds we keep in the sound buffer
	static const float _delay;

	static void soundCallback(void *data, u8 *stream, int length);
	virtual void read(u16 * stream, int length);
};

#endif // __VBA_SOUND_SDL_H__
