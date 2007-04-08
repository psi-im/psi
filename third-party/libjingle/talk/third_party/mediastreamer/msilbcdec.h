/*
  The mediastreamer library aims at providing modular media processing and I/O
	for linphone, but also for any telephony application.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org
  										
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


#ifndef MSILBCDECODER_H
#define MSILBCDECODER_H

#include <msfilter.h>
#include <mscodec.h>
#include <iLBC_decode.h>

/*this is the class that implements a ILBCdecoder filter*/

#define MSILBCDECODER_MAX_INPUTS  1 /* max output per filter*/


typedef struct _MSILBCDecoder
{
     /* the MSILBCDecoder derivates from MSFilter, so the MSFilter object MUST be the first of the MSILBCDecoder object
	in order to the object mechanism to work*/
     MSFilter filter;
     MSQueue *q_inputs[MSILBCDECODER_MAX_INPUTS];
     MSFifo *f_outputs[MSILBCDECODER_MAX_INPUTS];
     iLBC_Dec_Inst_t ilbc_dec;
     int bitrate;
     int ms_per_frame;
     int samples_per_frame;
     int bytes_per_compressed_frame;
} MSILBCDecoder;

typedef struct _MSILBCDecoderClass
{
	/* the MSILBCDecoder derivates from MSFilter, so the MSFilter class MUST be the first of the MSILBCDecoder class
       in order to the class mechanism to work*/
	MSFilterClass parent_class;
} MSILBCDecoderClass;

/* PUBLIC */

/* call this before if don't load the plugin dynamically */
void ms_ilbc_codec_init();

#define MS_ILBCDECODER(filter) ((MSILBCDecoder*)(filter))
#define MS_ILBCDECODER_CLASS(klass) ((MSILBCDecoderClass*)(klass))
MSFilter * ms_ilbc_decoder_new(void);

/* FOR INTERNAL USE*/
void ms_ilbc_decoder_init(MSILBCDecoder *r);
void ms_ilbc_decoder_class_init(MSILBCDecoderClass *klass);
void ms_ilbc_decoder_destroy( MSILBCDecoder *obj);
void ms_ilbc_decoder_process(MSILBCDecoder *r);

extern MSCodecInfo ilbc_info;

#endif
