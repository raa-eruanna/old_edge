//----------------------------------------------------------------------------
//  EDGE Linux Music System
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#include "ddf/main.h"
#include "ddf/playlist.h"


// FIXME !!!!!! temporary
#undef USE_OGG
#undef USE_HUMID

#include "s_sound.h"
#include "s_music.h"
#include "s_timid.h"
#include "unx_sysinc.h"


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

#ifdef USE_OGG
oggplayer_c *oggplayer = NULL;
#endif

#ifdef USE_HUMID
humdinger_c *humdinger = NULL;
#endif

#define MUSICERRLEN 256
static char errordesc[MUSICERRLEN];

bool musicpaused = false;


extern int dev_freq;
extern bool dev_stereo;


void I_StartupMusic(void)
{
	// Clear the error message
	memset(errordesc, '\0', sizeof(errordesc));

	if (nomusic) return;

	capable = 0;

#ifdef USE_OGG
	if (! nosound)
	{
		oggplayer = new oggplayer_c;
		capable |= support_OGG;

		I_Printf("I_StartupMusic: OGG Music Init OK\n");
	}
	else
#endif // USE_OGG
    {
		I_Printf("I_StartupMusic: OGG Music Disabled\n");
    }


#if 1
	if (! nosound)
	{
		if (S_StartupTimidity())
		{
			I_Printf("I_StartupMusic: Timidity Init OK\n");
			capable |= support_MUS | support_MIDI;
		}
		else
		{
			I_Printf("I_StartupMusic: Timidity Init FAILED\n");
		}
	}
	else
#endif
    {
		I_Printf("I_StartupMusic: Timidity Disabled\n");
    }

	// Music is not paused by default
	musicpaused = false;

	// Nothing went pear shaped
	return;
}

#if 0
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

            if (humdinger)
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
#else // !USE_HUMID
			I_PostMusicError("I_MusicPlayback: MUS/MIDI not supported.\n");
			handle = -1;
#endif
			break;
		}

		case MUS_OGG:
		{
#ifdef USE_OGG
            handle = -1;

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
#else // !USE_OGG
			I_PostMusicError("I_MusicPlayback: OGG-Vorbis not supported.\n");
			handle = -1;
#endif
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
#endif


void I_ShutdownMusic(void)
{
#ifdef USE_OGG
	if (oggplayer)
	{
		delete oggplayer;
		oggplayer = NULL;
	}
#endif

#ifdef USE_HUMID
	if (humdinger)
	{
		delete humdinger;
		humdinger = NULL;

		HumDingerTerm();
	}
#endif

}


void I_PostMusicError(const char *message)
{
	memset(errordesc, 0, MUSICERRLEN);

	if (strlen(message) > MUSICERRLEN-1)
		strncpy(errordesc, message, MUSICERRLEN-1);
	else
		strcpy(errordesc, message);

	return;
}


char *I_MusicReturnError(void)
{
	return errordesc;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab