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


#ifndef MSILBCENCODER_H
#define MSILBCENCODER_H

#include "mscodec.h"
#include <iLBC_encode.h>

#define ILBC_BITS_IN_COMPRESSED_FRAME 400

int
ilbc_read_16bit_samples(gint16 int16samples[], float speech[], int n);

int
ilbc_write_16bit_samples(gint16 int16samples[], float speech[], int n);

void
ilbc_write_bits(unsigned char *data, unsigned char *bits, int nbytes);

int
ilbc_read_bits(unsigned char *data, unsigned char *bits, int nbytes);


/*this is the class that implements a ILBCencoder filter*/

#define MSILBCENCODER_MAX_INPUTS  1 /* max output per filter*/


typedef struct _MSILBCEncoder
{
     /* the MSILBCEncoder derivates from MSFilter, so the MSFilter object MUST be the first of the MSILBCEncoder object
       in order to the object mechanism to work*/
     MSFilter filter;
     MSFifo *f_inputs[MSILBCENCODER_MAX_INPUTS];
     MSQueue *q_outputs[MSILBCENCODER_MAX_INPUTS];
     iLBC_Enc_Inst_t ilbc_enc;
     int ilbc_encoded_bytes;
     int bitrate;
     int ms_per_frame;
     int samples_per_frame;
     int bytes_per_compressed_frame;
} MSILBCEncoder;

typedef struct _MSILBCEncoderClass
{
	/* the MSILBCEncoder derivates from MSFilter, so the MSFilter class MUST be the first of the MSILBCEncoder class
       in order to the class mechanism to work*/
	MSFilterClass parent_class;
} MSILBCEncoderClass;

/* PUBLIC */
#define MS_ILBCENCODER(filter) ((MSILBCEncoder*)(filter))
#define MS_ILBCENCODER_CLASS(klass) ((MSILBCEncoderClass*)(klass))
MSFilter * ms_ilbc_encoder_new(void);

/* FOR INTERNAL USE*/
void ms_ilbc_encoder_init(MSILBCEncoder *r);
void ms_ilbc_encoder_class_init(MSILBCEncoderClass *klass);
void ms_ilbc_encoder_destroy( MSILBCEncoder *obj);
void ms_ilbc_encoder_process(MSILBCEncoder *r);

#define ILBC_MAX_BYTES_PER_COMPRESSED_FRAME NO_OF_BYTES_30MS
#define ILBC_MAX_SAMPLES_PER_FRAME BLOCKL_30MS

#endif
