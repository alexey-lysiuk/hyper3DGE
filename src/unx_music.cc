//----------------------------------------------------------------------------
//  EDGE Linux Music System
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2005  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
//
// -ACB- 2000/03/17 Added CD Music
// -ACB- 2001/01/14 Replaced I_WriteDebug() with I_PostMusicError()
//

#include "i_defs.h"
#include "unx_sysinc.h"

#include "oggplayer.h"

#ifdef USE_HUMID
#include "humdinger.h"
#endif

#include "s_sound.h"

#include <string.h>


// #defines for handle information
#define GETLIBHANDLE(_handle) (_handle&0xFF)
#define GETLOOPBIT(_handle)   ((_handle&0x10000)>>16)
#define GETTYPE(_handle)      ((_handle&0xFF00)>>8)

#define MAKEHANDLE(_type,_loopbit,_libhandle) \
	(((_loopbit&1)<<16)+(((_type)&0xFF)<<8)+(_libhandle))

typedef enum
{
	support_CD   = 0x01,
	support_MIDI = 0x02,
	support_MUS  = 0x04,
	support_OGG  = 0x08  // OGG Support - ACB- 2004/08/18
}
mussupport_e;

static byte capable;

oggplayer_c *oggplayer = NULL;

#ifdef USE_HUMID
humdinger_c *humdinger = NULL;
#endif

#define MUSICERRLEN 256
static char errordesc[MUSICERRLEN];

bool musicpaused = false;

int res_fx_handle;

//
// I_StartupMusic
//
bool I_StartupMusic(void *sysinfo)
{
	// Clear the error message
	memset(errordesc, '\0', sizeof(char)*MUSICERRLEN);

	if (nomusic)
		return true;

	capable = 0;
	if (!nocdmusic)
	{
		if (I_StartupCD())
			capable = support_CD;
		else
			I_Printf("%s\n", I_MusicReturnError());
	}

	if (! nosound)
	{
		oggplayer = new oggplayer_c;
		capable |= support_OGG;

		I_Printf("I_StartupMusic: OGG Music Init OK\n");
	}
	else
    {
		I_Printf("I_StartupMusic: OGG Music Disabled (no sound)\n");
    }

#ifdef USE_HUMID
	if (! nosound)
	{
		humdinger = HumDingerInit();

		if (humdinger)
		{
			I_Printf("I_StartupMusic: Humidity Init OK\n");
			capable |= support_MUS | support_MIDI;
		}
		else
		{
			I_Printf("I_StartupMusic: Humidity Init FAILED: %s\n",
				HumDingerGetError());
		}
	}
	else
    {
		I_Printf("I_StartupMusic: Humidity Disabled (no sound)\n");
    }
#endif  // USE_HUMID

	// Music is not paused by default
	musicpaused = false;

	// Nothing went pear shaped
	return true;
}

//
// I_MusicPlayback
//
int I_MusicPlayback(i_music_info_t *musdat, int type, bool looping,
	float gain)
{
	int handle;

	if (!(capable & support_CD)   && type == MUS_CD)   return -1;
	if (!(capable & support_MIDI) && type == MUS_MIDI) return -1;
	if (!(capable & support_MUS)  && type == MUS_MUS)  return -1;
	if (!(capable & support_OGG)  && type == MUS_OGG)  return -1;

	switch (type)
	{
		// CD Support...
		case MUS_CD:
		{
			if (! I_CDStartPlayback(musdat->info.cd.track, looping, gain))
				handle = -1;
			else
			{
				handle = MAKEHANDLE(MUS_CD, looping, musdat->info.cd.track);
			}
			break;
		}

		case MUS_MUS:
		case MUS_MIDI:
		{
#ifdef USE_HUMID
			handle = -1;

            res_fx_handle = 0;
            if (humdinger)
            {
                res_fx_handle = sound::ReserveFX(SNCAT_Music);
                if (res_fx_handle)
                {
                    int track = humdinger->Open((byte*)musdat->info.data.ptr,
                                                musdat->info.data.size);
                    
                    if (track == -1)
                    {
                        handle = -1;
                    }
                    else
                    {
                        humdinger->Play(looping, gain);
                        handle = MAKEHANDLE(type, looping, track);
                    }
                }
                else
                {
                    I_PostMusicError("I_MusicPlayback: No free channels for HUM playback.\n");
                    break;
                }
			}
#else
			I_PostMusicError("I_MusicPlayback: MUS/MIDI not supported.\n");
			handle = -1;
#endif
			break;
		}

		case MUS_OGG:
		{
            handle = -1;

            res_fx_handle = sound::ReserveFX(SNCAT_Music);
            if (!res_fx_handle)
            {
                I_PostMusicError("I_MusicPlayback: No free channels for OGG playback.\n");
                break;
            }

            if (musdat->format != IMUSSF_DATA &&
                musdat->format != IMUSSF_FILE)
            {
                I_PostMusicError("I_MusicPlayback: OGG given in unsupported format.\n");
                break;
            }

            if (musdat->format == IMUSSF_DATA)
				oggplayer->Open(musdat->info.data.ptr, musdat->info.data.size);
			else if (musdat->format == IMUSSF_FILE)
				oggplayer->Open(musdat->info.file.name);
				
			oggplayer->Play(looping, gain);
			handle = MAKEHANDLE(MUS_OGG, looping, 1);
			break;
		}

		case MUS_UNKNOWN:
		{
			I_PostMusicError("I_MusicPlayback: Unknown format type given.\n");
			handle = -1;
			break;
		}

		default:
		{
			I_PostMusicError("I_MusicPlayback: Weird Format given.\n");
			handle = -1;
			break;
		}
	}

	return handle;
}

//
// I_MusicPause
//
void I_MusicPause(int *handle)
{
	int type;

	if (*handle == -1)
		return;

	type = GETTYPE(*handle);

	switch (type)
	{
		case MUS_CD:
		{
			I_CDPausePlayback();
			break;
		}

#ifdef USE_HUMID
		case MUS_MUS:
		case MUS_MIDI:
		{
			if (humdinger) humdinger->Pause();
			break;
		}
#endif
		case MUS_OGG:
		{
			oggplayer->Pause();
			break;
		}

		default:
			break;
	}

	musicpaused = true;
}

//
// I_MusicResume
//
void I_MusicResume(int *handle)
{
	int type;

	if (*handle == -1)
		return;

	type = GETTYPE(*handle);

	switch (type)
	{
		case MUS_CD:
		{
			I_CDResumePlayback();
			break;
		}

#ifdef USE_HUMID
		case MUS_MUS:
		case MUS_MIDI:
		{
			if (humdinger) humdinger->Resume();
			break;
		}
#endif
		case MUS_OGG:
		{
			oggplayer->Resume();
			break;
		}

		default:
			break;
	}

	musicpaused = false;
}

//
// I_MusicKill
//
// You can't stop the rock!! This does...
//
void I_MusicKill(int *handle)
{
	int type;

	if (*handle == -1)
		return;

	type = GETTYPE(*handle);

	switch (type)
	{
		case MUS_CD:
		{
			I_CDStopPlayback();
			break;
		}

#ifdef USE_HUMID
		case MUS_MUS:
		case MUS_MIDI:
		{
			if (humdinger) 
            {
                humdinger->Close();
                sound::UnreserveFX(res_fx_handle);
            }
			break;
		}
#endif

		case MUS_OGG:
		{
			oggplayer->Close();
            sound::UnreserveFX(res_fx_handle);
			break;
		}

		default:
			break;
	}

	*handle = -1;
}

//
// I_SetMusicVolume
//
void I_SetMusicVolume(int *handle, float gain)
{
	int type;

	if (*handle == -1)
		return;

	type = GETTYPE(*handle);

	switch (type)
	{
		case MUS_CD:
		{
			I_CDSetVolume(gain);
			break;
		}

#ifdef USE_HUMID
		case MUS_MUS:
		case MUS_MIDI:
		{
			if (humdinger) humdinger->SetVolume(gain);
			break;
		}
#endif

		case MUS_OGG:
		{
			oggplayer->SetVolume(gain);
			break;
		}

		default:
			break;
	}
}

//
// I_MusicTicker
//
void I_MusicTicker(int *handle)
{
	int type;
	int libhandle;
	bool looping;

	if (musicpaused)
		return;

	if (*handle == -1)
		return;

	type = GETTYPE(*handle);
	looping = GETLOOPBIT(*handle);
	libhandle = GETLIBHANDLE(*handle);

	switch (type)
	{
		case MUS_CD:
		{
			if (I_GetTime() % TICRATE == 0)
			{
				if (! I_CDTicker())
					*handle = -1;
			}
			break;
		}

#ifdef USE_HUMID
		case MUS_MUS:
		case MUS_MIDI:
		{
			if (humdinger) humdinger->Ticker();
			break;
		}
#endif

		case MUS_OGG:
		{
			oggplayer->Ticker();
			break;
		}

		default:
			break;
	}
}

//
// I_ShutdownMusic
//
void I_ShutdownMusic(void)
{
	if (oggplayer)
	{
		delete oggplayer;
		oggplayer = NULL;
	}

#ifdef USE_HUMID
	if (humdinger)
	{
		delete humdinger;
		humdinger = NULL;

		HumDingerTerm();
	}
#endif

	I_ShutdownCD();
}

//
// I_PostMusicError
//
void I_PostMusicError(const char *message)
{
	memset(errordesc, 0, MUSICERRLEN);

	if (strlen(message) > MUSICERRLEN-1)
		strncpy(errordesc, message, MUSICERRLEN-1);
	else
		strcpy(errordesc, message);

	return;
}

//
// I_MusicReturnError
//
char *I_MusicReturnError(void)
{
	return errordesc;
}

