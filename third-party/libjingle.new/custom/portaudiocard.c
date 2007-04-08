/*
  Copyright (C) 2005 Remko Troncon
  										
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <portaudio.h>
#include <stdlib.h>

#include "portaudiocard.h"
#include "msossread.h"
#include "msosswrite.h"

// Settings
#define BUFFER_SIZE 2048

// PortAudio settings
#define FRAMES_PER_BUFFER 256


// -----------------------------------------------------------------------------

int readBuffer(char* buffer, char** buffer_read_p, char*buffer_write, char* buffer_end, char* target_buffer, int target_len)
{
	char *end, *tmp, *buffer_read = *buffer_read_p;
	size_t remaining, len;
	int read = 0;
	
	// First phase
	tmp = buffer_read + target_len;
	if (buffer_write < buffer_read) {
		if (tmp > buffer_end) {
			end = buffer_end;
			remaining = tmp - buffer_end;
		}
		else {
			end = tmp;
			remaining = 0;
		}
	}
	else {
		end = (tmp >= buffer_write ? buffer_write : tmp);
		remaining = 0;
	}
	//printf("end: %p\n",end);

	// Copy the data
	len = end - buffer_read;
	memcpy(target_buffer, buffer_read, len);
	buffer_read += len;
	target_buffer += len;
	read += len;

	// Second phase
	if (remaining > 0) {
		buffer_read = buffer;
		tmp = buffer_read + remaining;
		len = (tmp > buffer_write ? buffer_write : tmp) - buffer_read;
		memcpy(target_buffer, buffer_read, len);
		buffer_read += len;
		read += len;
	}

	// Finish up
	*buffer_read_p = buffer_read;

	return read;
}

int writeBuffer(char* buffer, char* buffer_read, char** buffer_write_p, char* buffer_end, char* source_buffer, int source_len)
{
	char *end, *tmp, *buffer_write = *buffer_write_p;
	size_t remaining, len;
	int written = 0;

	// First phase
	tmp = buffer_write + source_len;
	if (buffer_write >= buffer_read) {
		if (tmp > buffer_end) {
			end = buffer_end;
			remaining = tmp - buffer_end;
		}
		else {
			end = tmp;
			remaining = 0;
		}
	}
	else {
		if (tmp > buffer_read) {
			printf("Warning: Dropping frame(s) %p %p\n", tmp, buffer_read);
			end = buffer_read;
			remaining = 0;
		}
		else {
			end = tmp;
			remaining = 0;
		}
	}
	
	len = end - buffer_write;
	memcpy(buffer_write, source_buffer, len);
	buffer_write += len;
	source_buffer += len;
	written += len;
	
	// Second phase
	if (remaining > 0) {
		buffer_write = buffer;
		tmp = buffer_write + remaining;
		if (tmp > buffer_read) {
			printf("Warning: Dropping frame(s) %p %p\n", tmp, buffer_read);
			end = buffer_read;
		}
		else {
			end = tmp;
		}
		
		len = end - buffer_write;
		memcpy(buffer_write, source_buffer, len);
		buffer_write += len;
		written += len;
	}

	// Finish up
	*buffer_write_p = buffer_write;
	return written;
}

// -----------------------------------------------------------------------------

static int portAudioCallback( void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, PaTimestamp outTime, void *card_p )
{
	PortAudioCard* card = (PortAudioCard*) card_p;

	size_t len = framesPerBuffer * Pa_GetSampleSize(paInt16); 
	//printf("PA::readBuffer begin %p %p %p %p %d\n",card->out_buffer,card->out_buffer_read,card->out_buffer_write,card->out_buffer_end, len);
	readBuffer(card->out_buffer,&card->out_buffer_read,card->out_buffer_write,card->out_buffer_end, outputBuffer, len);
	//printf("PA::readBuffer end %p %p %p %p %d\n",card->out_buffer,card->out_buffer_read,card->out_buffer_write,card->out_buffer_end, len);
	writeBuffer(card->in_buffer,card->in_buffer_read,&card->in_buffer_write,card->in_buffer_end, inputBuffer, len);
	return 0;
}

// -----------------------------------------------------------------------------

int portaudio_card_probe(PortAudioCard *obj, int bits, int stereo, int rate)
{
	return FRAMES_PER_BUFFER * (SND_CARD(obj)->stereo ? 2 : 1) * Pa_GetSampleSize(paInt16);
}


int portaudio_card_open_r(PortAudioCard *obj,int bits,int stereo,int rate)
{
	fprintf(stderr,"Opening PortAudio card\n");

	int err;
	err = Pa_OpenDefaultStream(&obj->stream, 1, 1, paInt16, rate, FRAMES_PER_BUFFER, 0, portAudioCallback, obj);
	if (err != paNoError) {
		fprintf(stderr, "Error creating a PortAudio stream: %s\n", Pa_GetErrorText(err));
		return -1;
	}

	err = Pa_StartStream(obj->stream);
	if (err != paNoError) {
		fprintf(stderr, "Error starting PortAudio stream: %s\n", Pa_GetErrorText(err));
		Pa_CloseStream(obj->stream);
		obj->stream = NULL;
		return -1;
	}

	SND_CARD(obj)->bits = 16;
	SND_CARD(obj)->stereo = 0;
	SND_CARD(obj)->rate = rate;
	// Should this be multiplied by Pa_GetMinNumBuffers(FRAMES_PER_BUFFER,sampleRate) ?
	SND_CARD(obj)->bsize = FRAMES_PER_BUFFER * (SND_CARD(obj)->stereo ? 2 : 1) * Pa_GetSampleSize(paInt16);

	return 0;

}

void portaudio_card_close_r(PortAudioCard *obj)
{
	fprintf(stderr, "Closing PortAudio card\n");
	if (obj->stream) {
		Pa_StopStream(obj->stream);
		Pa_CloseStream(obj->stream);
		obj->stream = NULL;
	}
}

int portaudio_card_open_w(PortAudioCard *obj,int bits,int stereo,int rate)
{
}

void portaudio_card_close_w(PortAudioCard *obj)
{
}

void portaudio_card_destroy(PortAudioCard *obj)
{
	snd_card_uninit(SND_CARD(obj));
	free(obj->in_buffer);
	free(obj->out_buffer);
}

gboolean portaudio_card_can_read(PortAudioCard *obj)
{
	return obj->in_buffer_read != obj->in_buffer_write;
}

int portaudio_card_read(PortAudioCard *obj,char *buf,int size)
{
	//printf("read begin %p %p %p %p %d\n",obj->in_buffer,obj->in_buffer_read,obj->in_buffer_write,obj->in_buffer_end, size);
	return readBuffer(obj->in_buffer,&obj->in_buffer_read,obj->in_buffer_write,obj->in_buffer_end, buf, size);
	//printf("read end %p %p %p %p %d\n",obj->in_buffer,obj->in_buffer_read,obj->in_buffer_write,obj->in_buffer_end, size);
}

int portaudio_card_write(PortAudioCard *obj,char *buf,int size)
{
	//printf("writeBuffer begin %p %p %p %p %d\n",obj->out_buffer,obj->out_buffer_read,obj->out_buffer_write,obj->out_buffer_end, size);
	return writeBuffer(obj->out_buffer,obj->out_buffer_read,&obj->out_buffer_write,obj->out_buffer_end, buf, size);
	//printf("writeBuffer end %p %p %p %p %d\n",obj->out_buffer,obj->out_buffer_read,obj->out_buffer_write,obj->out_buffer_end, size);
}

void portaudio_card_set_level(PortAudioCard *obj,gint way,gint a)
{
}

gint portaudio_card_get_level(PortAudioCard *obj,gint way)
{
	return 0;
}

void portaudio_card_set_source(PortAudioCard *obj,int source)
{
}

MSFilter *portaudio_card_create_read_filter(PortAudioCard *card)
{
	MSFilter *f=ms_oss_read_new();
	ms_oss_read_set_device(MS_OSS_READ(f),SND_CARD(card)->index);
	return f;
}

MSFilter *portaudio_card_create_write_filter(PortAudioCard *card)
{
	MSFilter *f=ms_oss_write_new();
	ms_oss_write_set_device(MS_OSS_WRITE(f),SND_CARD(card)->index);
	return f;
}


SndCard* portaudio_card_new()
{
	// Basic stuff
	PortAudioCard* obj= g_new0(PortAudioCard,1);
	SndCard* base= SND_CARD(obj);
	snd_card_init(base);
	base->card_name=g_strdup_printf("PortAudio Card");
	base->_probe=(SndCardOpenFunc)portaudio_card_probe;
	base->_open_r=(SndCardOpenFunc)portaudio_card_open_r;
	base->_open_w=(SndCardOpenFunc)portaudio_card_open_w;
	base->_can_read=(SndCardPollFunc)portaudio_card_can_read;
	base->_read=(SndCardIOFunc)portaudio_card_read;
	base->_write=(SndCardIOFunc)portaudio_card_write;
	base->_close_r=(SndCardCloseFunc)portaudio_card_close_r;
	base->_close_w=(SndCardCloseFunc)portaudio_card_close_w;
	base->_set_rec_source=(SndCardMixerSetRecSourceFunc)portaudio_card_set_source;
	base->_set_level=(SndCardMixerSetLevelFunc)portaudio_card_set_level;
	base->_get_level=(SndCardMixerGetLevelFunc)portaudio_card_get_level;
	base->_destroy=(SndCardDestroyFunc)portaudio_card_destroy;
	base->_create_read_filter=(SndCardCreateFilterFunc)portaudio_card_create_read_filter;
	base->_create_write_filter=(SndCardCreateFilterFunc)portaudio_card_create_write_filter;

	// Initialize stream
	obj->stream = NULL;
	
	// Initialize buffers
	obj->out_buffer = (char*) malloc(sizeof(char)*BUFFER_SIZE);
	obj->out_buffer_read = obj->out_buffer_write = obj->out_buffer;
	obj->out_buffer_end = obj->out_buffer + BUFFER_SIZE;
	obj->in_buffer = (char*) malloc(sizeof(char)*BUFFER_SIZE);
	obj->in_buffer_read = obj->in_buffer_write = obj->in_buffer;
	obj->in_buffer_end = obj->in_buffer + BUFFER_SIZE;

	return base;
}

gint portaudio_card_manager_init(SndCardManager *manager, gint tabindex)
{
	// Initialize portaudio lib
	int err = Pa_Initialize();
	if (err != paNoError) {
		fprintf(stderr,"Error initializing PortAudio: %s\n",Pa_GetErrorText(err));
		return 0;
	}
	
	// Create new card
	manager->cards[0]=portaudio_card_new();
	manager->cards[0]->index=0;
	
	return 1;
}
