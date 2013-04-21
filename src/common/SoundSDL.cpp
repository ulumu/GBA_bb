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

#include "SoundSDL.h"

extern int emulating;
extern bool speedup;

// Hold up to 100 ms of data in the ring buffer
const float SoundSDL::_delay = 0.800f;

SoundSDL::SoundSDL():
	_rbuf(0),
	_initialized(false)
{

}

void SoundSDL::soundCallback(void *data, u8 *stream, int len)
{
	reinterpret_cast<SoundSDL*>(data)->read(reinterpret_cast<u16 *>(stream), len);
}

void SoundSDL::read(u16 * stream, int length)
{
	if (!_initialized || length <= 0 || !emulating)
		return;

	SDL_mutexP(_mutex);

	_rbuf.read(stream, std::min(static_cast<std::size_t>(length) / 2, _rbuf.used()));

	SDL_CondSignal(_cond);
	SDL_mutexV(_mutex);
}

inline void SoundSDL::write(u16 * finalWave, int length)
{
	unsigned int samples = length / 4;
	std::size_t avail;

	if (!_initialized || length <= 0 || !emulating)
		return;
/*
	if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING)
		SDL_PauseAudio(0);

*/
	SDL_mutexP(_mutex);

	while ((avail = _rbuf.avail() / 2) < samples)
	{
		_rbuf.write(finalWave, avail * 2);
		finalWave += avail * 2;
		samples -= avail;

		// If emulating and not in speed up mode, synchronize to audio
		// by waiting till there is enough room in the buffer

#ifdef __QNXNTO__  // forced sync is glitchy .. need performance improvements...
//		if (emulating && !speedup)
//		{
//			SDL_CondWait(_cond, _mutex);
//		}
//		else
#endif
		{
			// Drop the remaining of the audio data
			SDL_mutexV(_mutex);

			SLOG("Drop audio:%d", samples);

			return;
		}
	}

	_rbuf.write(finalWave, samples * 2);

	SDL_mutexV(_mutex);
}

#define FRAME_DURATION 50 // 200ms audio frame

bool SoundSDL::init(long sampleRate)
{
	SDL_AudioSpec wantAudioSpec, gotAudioSpec;

	wantAudioSpec.freq     = sampleRate;
	wantAudioSpec.format   = AUDIO_S16SYS;
	wantAudioSpec.channels = 2;
	wantAudioSpec.samples  = 1024;
//    wantAudioSpec.samples  = (sampleRate * wantAudioSpec.channels * FRAME_DURATION) / 1000;
	wantAudioSpec.callback = soundCallback;
	wantAudioSpec.userdata = this;

	SLOG("Wanted:: freq=%d, format=%X, channels=%d, samples=%d", wantAudioSpec.freq, wantAudioSpec.format, wantAudioSpec.channels, wantAudioSpec.samples);

	if(SDL_OpenAudio(&wantAudioSpec, &gotAudioSpec))
	{
		SLOG("Failed to open audio: %s", SDL_GetError());
		return false;
	}

	SLOG("Got:: freq=%d, format=%X, channels=%d, samples=%d", gotAudioSpec.freq, gotAudioSpec.format, gotAudioSpec.channels, gotAudioSpec.samples);

	_rbuf.reset(_delay * sampleRate * 2);

	_cond  = SDL_CreateCond();
	_mutex = SDL_CreateMutex();
	_initialized = true;

	return true;
}

SoundSDL::~SoundSDL()
{
	if (!_initialized)
		return;

	SDL_mutexP(_mutex);
	int iSave = emulating;
	emulating = 0;
	SDL_CondSignal(_cond);
	SDL_mutexV(_mutex);

	SDL_DestroyCond(_cond);
	_cond = NULL;

	SDL_DestroyMutex(_mutex);
	_mutex = NULL;

	SDL_CloseAudio();

	emulating = iSave;
}

void SoundSDL::pause()
{
	if (!_initialized)
		return;

	SDL_PauseAudio(1);
}

void SoundSDL::resume()
{
	if (!_initialized)
		return;

	SDL_PauseAudio(0);
}

void SoundSDL::reset()
{
}
