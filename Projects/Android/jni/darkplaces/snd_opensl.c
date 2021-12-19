/*
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

//Updated by Emile Belanger for OpenSL

// for native audio
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <pthread.h>


static unsigned int sdlaudiotime = 0;


// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;

// output mix interfaces
static SLObjectItf outputMixObject = NULL;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay;
#ifdef ANDROID_NDK
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
#else
static SLBufferQueueItf bqPlayerBufferQueue;
#endif

static SLEffectSendItf bqPlayerEffectSend;
static SLMuteSoloItf bqPlayerMuteSolo;
static SLVolumeItf bqPlayerVolume;

void assert(int v,const char * message)
{
//	if (!v)
//		LOGI("ASSERT: %s",message);
}
/*
// Note: SDL calls SDL_LockAudio() right before this function, so no need to lock the audio data here
static void Buffer_Callback (void *userdata, unsigned char *stream, int len)
{
	unsigned int factor, RequestedFrames, MaxFrames, FrameCount;
	unsigned int StartOffset, EndOffset;

	factor = snd_renderbuffer->format.channels * snd_renderbuffer->format.width;
	if ((unsigned int)len % factor != 0)
		Sys_Error("SDL sound: invalid buffer length passed to Buffer_Callback (%d bytes)\n", len);

	RequestedFrames = (unsigned int)len / factor;

	if (SndSys_LockRenderBuffer())
	{
		if (snd_usethreadedmixing)
		{
			S_MixToBuffer(stream, RequestedFrames);
			if (snd_blocked)
				memset(stream, snd_renderbuffer->format.width == 1 ? 0x80 : 0, len);
			SndSys_UnlockRenderBuffer();
			return;
		}

		// Transfert up to a chunk of samples from snd_renderbuffer to stream
		MaxFrames = snd_renderbuffer->endframe - snd_renderbuffer->startframe;
		if (MaxFrames > RequestedFrames)
			FrameCount = RequestedFrames;
		else
			FrameCount = MaxFrames;
		StartOffset = snd_renderbuffer->startframe % snd_renderbuffer->maxframes;
		EndOffset = (snd_renderbuffer->startframe + FrameCount) % snd_renderbuffer->maxframes;
		if (StartOffset > EndOffset)  // if the buffer wraps
		{
			unsigned int PartialLength1, PartialLength2;

			PartialLength1 = (snd_renderbuffer->maxframes - StartOffset) * factor;
			memcpy(stream, &snd_renderbuffer->ring[StartOffset * factor], PartialLength1);

			PartialLength2 = FrameCount * factor - PartialLength1;
			memcpy(&stream[PartialLength1], &snd_renderbuffer->ring[0], PartialLength2);
		}
		else
			memcpy(stream, &snd_renderbuffer->ring[StartOffset * factor], FrameCount * factor);

		snd_renderbuffer->startframe += FrameCount;

		if (FrameCount < RequestedFrames && developer_insane.integer && vid_activewindow)
			Con_DPrintf("SDL sound: %u sample frames missing\n", RequestedFrames - FrameCount);

		sdlaudiotime += RequestedFrames;

		SndSys_UnlockRenderBuffer();
	}
}

 */

#define OPENSL_BUFF_LEN 1024

static unsigned char play_buffer[OPENSL_BUFF_LEN];

//NOTE!! There are definetly threading issues with this, but it appears to work for now...

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	//LOGI("bqPlayerCallback");
	unsigned int factor, RequestedFrames, MaxFrames, FrameCount;
	unsigned int StartOffset, EndOffset;

	factor = snd_renderbuffer->format.channels * snd_renderbuffer->format.width;

	RequestedFrames = (unsigned int)OPENSL_BUFF_LEN / factor;
	if (SndSys_LockRenderBuffer())
	{
		if (snd_usethreadedmixing)
		{
			S_MixToBuffer(play_buffer, RequestedFrames);
			if (snd_blocked)
				memset(play_buffer, snd_renderbuffer->format.width == 1 ? 0x80 : 0, OPENSL_BUFF_LEN);

			(*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, play_buffer,OPENSL_BUFF_LEN);

			SndSys_UnlockRenderBuffer();
			return;
		}

		// Transfert up to a chunk of samples from snd_renderbuffer to stream
		MaxFrames = snd_renderbuffer->endframe - snd_renderbuffer->startframe;
		if (MaxFrames > RequestedFrames)
			FrameCount = RequestedFrames;
		else
			FrameCount = MaxFrames;
		StartOffset = snd_renderbuffer->startframe % snd_renderbuffer->maxframes;
		EndOffset = (snd_renderbuffer->startframe + FrameCount) % snd_renderbuffer->maxframes;
		if (StartOffset > EndOffset)  // if the buffer wraps
		{
			unsigned int PartialLength1, PartialLength2;

			PartialLength1 = (snd_renderbuffer->maxframes - StartOffset) * factor;
			memcpy(play_buffer, &snd_renderbuffer->ring[StartOffset * factor], PartialLength1);

			PartialLength2 = FrameCount * factor - PartialLength1;
			memcpy(&play_buffer[PartialLength1], &snd_renderbuffer->ring[0], PartialLength2);
		}
		else
			memcpy(play_buffer, &snd_renderbuffer->ring[StartOffset * factor], FrameCount * factor);

		snd_renderbuffer->startframe += FrameCount;

		if (FrameCount < RequestedFrames && developer_insane.integer && vid_activewindow)
			Con_DPrintf("SDL sound: %u sample frames missing\n", RequestedFrames - FrameCount);

		sdlaudiotime += RequestedFrames;

		SndSys_UnlockRenderBuffer();
	}

	SLresult result;
	//LOGI("Frame count = %d",FrameCount);
	if (FrameCount == 0)
		FrameCount = 1;
	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, play_buffer,FrameCount * factor);
	assert(SL_RESULT_SUCCESS == result,"Enqueue failed");

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
	unsigned int buffersize;

	snd_threaded = false;

	Con_DPrint ("SndSys_Init: using the SDL module\n");



	buffersize = CeilPowerOf2((unsigned int)ceil((double)requested->speed / 25.0)); // 2048 bytes on 24kHz to 48kHz


	Con_Printf("Wanted audio Specification:\n"
			"\tChannels  : %i\n"
			"\tFormat    : 0x%X\n"
			"\tFrequency : %i\n"
			"\tSamples   : %i\n",
			requested->channels, requested->width, requested->speed, buffersize);

	//Opensl only does 44100
	if (suggested != NULL)
	{
		suggested->channels = 2;
		suggested->width = 2;
		suggested->speed = 44100;
	}

	SLresult result;

	// create engine
	result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
	assert(SL_RESULT_SUCCESS == result,"slCreateEngine");

	// realize the engine
	result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result,"Realize");

	// get the engine interface, which is needed in order to create other objects
	result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
	assert(SL_RESULT_SUCCESS == result,"GetInterface");

	// create output mix
	result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
	assert(SL_RESULT_SUCCESS == result,"CreateOutputMix");

	// realize the output mix
	result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result,"Realize output mix");

	//CREATE THE PLAYER

	// configure audio source
	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1};
	SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
			SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
			SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};
	SLDataSource audioSrc = {&loc_bufq, &format_pcm};

	// configure audio sink
	SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
	SLDataSink audioSnk = {&loc_outmix, NULL};

	// create audio player
//	LOGI("create audio player");
	const SLInterfaceID ids[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
	const SLboolean req[1] = {SL_BOOLEAN_TRUE};
	result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
			1, ids, req);
	assert(SL_RESULT_SUCCESS == result,"CreateAudioPlayer");


	// realize the player
	result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result,"Realize AudioPlayer");

	// get the play interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
	assert(SL_RESULT_SUCCESS == result,"GetInterface AudioPlayer");

	// get the buffer queue interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
			&bqPlayerBufferQueue);
	assert(SL_RESULT_SUCCESS == result,"GetInterface buffer queue");

	// register callback on the buffer queue
	result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL);
	assert(SL_RESULT_SUCCESS == result,"RegisterCallback");

	// set the player's state to playing
	result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
	assert(SL_RESULT_SUCCESS == result,"SetPlayState");




	snd_threaded = true;

	snd_renderbuffer = Snd_CreateRingBuffer(requested, 0, NULL);
	if (snd_channellayout.integer == SND_CHANNELLAYOUT_AUTO)
		Cvar_SetValueQuick (&snd_channellayout, SND_CHANNELLAYOUT_STANDARD);

	sdlaudiotime = 0;

	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, "\0", 1);
	assert(SL_RESULT_SUCCESS == result,"Enqueue first buffer");



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

	/* Make sure player is stopped */
	(*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);

	 /* Destroy the player */
	 (*engineObject)->Destroy(engineObject);
	/* Destroy Output Mix object */
	 (*outputMixObject)->Destroy(outputMixObject);


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
	return sdlaudiotime;
}


/*
====================
SndSys_LockRenderBuffer

Get the exclusive lock on "snd_renderbuffer"
====================
 */
qboolean SndSys_LockRenderBuffer (void)
{
	//SDL_LockAudio();
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
	//SDL_UnlockAudio();
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
