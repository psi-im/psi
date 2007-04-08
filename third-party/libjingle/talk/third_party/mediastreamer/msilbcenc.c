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

#include <config.h>

#ifdef HAVE_ILBC

#include <stdlib.h>
#include <stdio.h>
#include "msilbcenc.h"


extern MSCodecInfo ilbc_info;

/* The return value of each of these calls is the same as that
   returned by fread/fwrite, which should be the number of samples
   successfully read/written, not the number of bytes. */

int
ilbc_read_16bit_samples(gint16 int16samples[], float speech[], int n)
{
    int i;

    /* Convert 16 bit integer samples to floating point values in the
       range [-1,+1]. */

    for (i = 0; i < n; i++) {
        speech[i] = int16samples[i];
    }

    return (n);
}



int
ilbc_write_16bit_samples(gint16 int16samples[], float speech[], int n)
{
	int i;
	float real_sample;

	/* Convert floating point samples in range [-1,+1] to 16 bit
	   integers. */
	for (i = 0; i < n; i++) {
		float dtmp=speech[i]; 
		if (dtmp<MIN_SAMPLE) 
			dtmp=MIN_SAMPLE; 
		else if (dtmp>MAX_SAMPLE) 
			dtmp=MAX_SAMPLE; 
		int16samples[i] = (short) dtmp; 
	}
	return (n);
}

/*

Write the bits in bits[0] through bits[len-1] to file f, in "packed"
format.

bits is expected to be an array of len integer values, where each
integer is 0 to represent a 0 bit, and any other value represents a 1
bit.  This bit string is written to the file f in the form of several
8 bit characters.  If len is not a multiple of 8, then the last
character is padded with 0 bits -- the padding is in the least
significant bits of the last byte.  The 8 bit characters are "filled"
in order from most significant bit to least significant.

*/

void
ilbc_write_bits(unsigned char *data, unsigned char *bits, int nbytes)
{
	memcpy(data, bits, nbytes);
}



/*

Read bits from file f into bits[0] through bits[len-1], in "packed"
format.

*/

int
ilbc_read_bits(unsigned char *data, unsigned char *bits, int nbytes)
{

	memcpy(bits, data, nbytes);

	return (nbytes);
}




static MSILBCEncoderClass *ms_ilbc_encoder_class=NULL;

MSFilter * ms_ilbc_encoder_new(void)
{
	MSILBCEncoder *r;
	
	r=g_new(MSILBCEncoder,1);
	ms_ilbc_encoder_init(r);
	if (ms_ilbc_encoder_class==NULL)
	{
		ms_ilbc_encoder_class=g_new(MSILBCEncoderClass,1);
		ms_ilbc_encoder_class_init(ms_ilbc_encoder_class);
	}
	MS_FILTER(r)->klass=MS_FILTER_CLASS(ms_ilbc_encoder_class);
	return(MS_FILTER(r));
}
	

int ms_ilbc_encoder_set_property(MSILBCEncoder *obj, MSFilterProperty prop, char *value)
{
	switch(prop){
		case MS_FILTER_PROPERTY_FMTP:
			if (value == NULL) return 0;
			if (strstr(value,"ptime=20")!=NULL) obj->ms_per_frame=20;
			else if (strstr(value,"ptime=30")!=NULL) obj->ms_per_frame=30;
			else g_warning("Unrecognized fmtp parameter for ilbc encoder!");
		break;
	}
	return 0;
}


int ms_ilbc_encoder_get_property(MSILBCEncoder *obj, MSFilterProperty prop, char *value)
{
	switch(prop){
		case MS_FILTER_PROPERTY_FMTP:
			if (obj->ms_per_frame==20) strncpy(value,"ptime=20",MS_FILTER_PROPERTY_STRING_MAX_SIZE);
			if (obj->ms_per_frame==30) strncpy(value,"ptime=30",MS_FILTER_PROPERTY_STRING_MAX_SIZE);
		break;
	}
	return 0;
}

void ms_ilbc_encoder_setup(MSILBCEncoder *r) 
{
	MSFilterClass *klass = NULL;
	switch (r->ms_per_frame) {
	case 20:
		r->samples_per_frame = BLOCKL_20MS;
		r->bytes_per_compressed_frame = NO_OF_BYTES_20MS;
		break;
	case 30:
		r->samples_per_frame = BLOCKL_30MS;
		r->bytes_per_compressed_frame = NO_OF_BYTES_30MS;
		break;
	default:
		g_error("Bad bitrate value (%i) for ilbc encoder!", r->ms_per_frame);
		break;
	}
	MS_FILTER(r)->r_mingran= (r->samples_per_frame * 2);
	g_message("Using ilbc encoder with %i ms frames mode.",r->ms_per_frame);
	initEncode(&r->ilbc_enc, r->ms_per_frame /* ms frames */);
}

/* FOR INTERNAL USE*/
void ms_ilbc_encoder_init(MSILBCEncoder *r)
{
	/* default bitrate */
	r->bitrate = 15200;
	r->ms_per_frame = 20;
	r->samples_per_frame = BLOCKL_20MS;
	r->bytes_per_compressed_frame = NO_OF_BYTES_20MS;

	ms_filter_init(MS_FILTER(r));
	MS_FILTER(r)->infifos=r->f_inputs;
	MS_FILTER(r)->outqueues=r->q_outputs;
	MS_FILTER(r)->r_mingran= (r->samples_per_frame * 2);
	memset(r->f_inputs,0,sizeof(MSFifo*)*MSILBCENCODER_MAX_INPUTS);
	memset(r->q_outputs,0,sizeof(MSFifo*)*MSILBCENCODER_MAX_INPUTS);
}

void ms_ilbc_encoder_class_init(MSILBCEncoderClass *klass)
{
	ms_filter_class_init(MS_FILTER_CLASS(klass));
	ms_filter_class_set_name(MS_FILTER_CLASS(klass),"ILBCEnc");
	MS_FILTER_CLASS(klass)->max_finputs=MSILBCENCODER_MAX_INPUTS;
	MS_FILTER_CLASS(klass)->max_qoutputs=MSILBCENCODER_MAX_INPUTS;
	MS_FILTER_CLASS(klass)->r_maxgran=ILBC_MAX_SAMPLES_PER_FRAME*2;
	MS_FILTER_CLASS(klass)->set_property=(MSFilterPropertyFunc)ms_ilbc_encoder_set_property;
	MS_FILTER_CLASS(klass)->get_property=(MSFilterPropertyFunc)ms_ilbc_encoder_get_property;
	MS_FILTER_CLASS(klass)->setup=(MSFilterSetupFunc)ms_ilbc_encoder_setup;
	MS_FILTER_CLASS(klass)->destroy=(MSFilterDestroyFunc)ms_ilbc_encoder_destroy;
	MS_FILTER_CLASS(klass)->process=(MSFilterProcessFunc)ms_ilbc_encoder_process;
	MS_FILTER_CLASS(klass)->info=(MSFilterInfo*)&ilbc_info;
}
	
void ms_ilbc_encoder_process(MSILBCEncoder *r)
{
	MSFifo *fi;
	MSQueue *qo;
	MSMessage *m;
	void *src=NULL;
	float speech[ILBC_MAX_SAMPLES_PER_FRAME];
	
	/* process output fifos, but there is only one for this class of filter*/
	
	qo=r->q_outputs[0];
	fi=r->f_inputs[0];
	ms_fifo_get_read_ptr(fi,r->samples_per_frame*2,&src);
	if (src==NULL) {
		g_warning( "src=%p\n", src);
		return;
	}
	m=ms_message_new(r->bytes_per_compressed_frame);

	ilbc_read_16bit_samples((gint16*)src, speech, r->samples_per_frame);
	iLBC_encode((unsigned char *)m->data, speech, &r->ilbc_enc);
	ms_queue_put(qo,m);
}

void ms_ilbc_encoder_uninit(MSILBCEncoder *obj)
{
}

void ms_ilbc_encoder_destroy( MSILBCEncoder *obj)
{
	ms_ilbc_encoder_uninit(obj);
	g_free(obj);
}

#endif
