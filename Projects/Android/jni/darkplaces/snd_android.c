/*
Copyright (C) 2013 n0n3m4
Copyright (C) 2004 Andreas Kirsch

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include "quakedef.h"

#include <math.h>

#include "snd_main.h"


static unsigned int audiopos = 0;
static unsigned int buffersize;
//__attribute__((weak)) __dso_handle=0;

void jni_initAudio(void *buffer, int size);
void jni_writeAudio(int offset, int length);

void QC_GetAudio()
{
    int offset = (audiopos*4) & (buffersize - 1);
    if (snd_renderbuffer!=NULL)
    	jni_writeAudio(offset, 2048*4);
    audiopos+=2048;
}

/*
====================
SndSys_Init

Create "snd_renderbuffer" with the proper sound format if the call is successful
May return a suggested format if the requested format isn't available
====================
*/
qboolean SndSys_Init (const snd_format_t* requested, snd_format_t* suggested)
{

	snd_threaded = false;

	Con_DPrint ("SndSys_Init: using the ANDROID module\n");

	int wantspecfreq = requested->speed;
	int wantspecformat = (requested->width);
	int wantspecchannels = requested->channels;

	Con_Printf("Wanted audio Specification:\n"
				"\tChannels  : %i\n"
				"\tFormat    : 0x%X\n"
				"\tFrequency : %i\n",
				wantspecchannels, wantspecformat, wantspecfreq);

	int obtainspecchannels=2;
	int obtainspecfreq=44100;
	int obtainspecformat=2;

	Con_Printf("Obtained audio specification:\n"
				"\tChannels  : %i\n"
				"\tFormat    : 0x%X\n"
				"\tFrequency : %i\n",
				obtainspecchannels, obtainspecformat, obtainspecfreq);

	// If we haven't obtained what we wanted
	if (wantspecfreq != obtainspecfreq ||
		wantspecformat != obtainspecformat ||
		wantspecchannels != obtainspecchannels)
	{
		// Pass the obtained format as a suggested format
		if (suggested != NULL)
		{
			suggested->speed = obtainspecfreq;
			suggested->width = obtainspecformat;
			suggested->channels = obtainspecchannels;
		}

		return false;
	}

	snd_threaded = false; //TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	snd_renderbuffer = Snd_CreateRingBuffer(requested, 16384, 0);
	if (snd_channellayout.integer == SND_CHANNELLAYOUT_AUTO)
		Cvar_SetValueQuick (&snd_channellayout, SND_CHANNELLAYOUT_STANDARD);

	audiopos = 0;
	buffersize=16384*2*2;
	jni_initAudio(snd_renderbuffer->ring, buffersize);

	return true;
}


/*
====================
SndSys_Shutdown

Stop the sound card, delete "snd_renderbuffer" and free its other resources
====================
*/
void SndSys_Shutdown(void)
{
	if (snd_renderbuffer != NULL)
	{
		Mem_Free(snd_renderbuffer->ring);
		Mem_Free(snd_renderbuffer);
		snd_renderbuffer = NULL;
	}
}


/*
====================
SndSys_Submit

Submit the contents of "snd_renderbuffer" to the sound card
====================
*/
void SndSys_Submit (void)
{
	// Nothing to do here (this sound module is callback-based)
}


/*
====================
SndSys_GetSoundTime

Returns the number of sample frames consumed since the sound started
====================
*/
unsigned int SndSys_GetSoundTime (void)
{
	return audiopos;
}


/*
====================
SndSys_LockRenderBuffer

Get the exclusive lock on "snd_renderbuffer"
====================
*/
qboolean SndSys_LockRenderBuffer (void)
{
	return true;
}


/*
====================
SndSys_UnlockRenderBuffer

Release the exclusive lock on "snd_renderbuffer"
====================
*/
void SndSys_UnlockRenderBuffer (void)
{
}

/*
====================
SndSys_SendKeyEvents

Send keyboard events originating from the sound system (e.g. MIDI)
====================
*/
void SndSys_SendKeyEvents(void)
{
	// not supported
}
