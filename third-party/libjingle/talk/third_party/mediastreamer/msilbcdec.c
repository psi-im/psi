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


#include "msilbcdec.h"
#include "msilbcenc.h"
#include "mscodec.h"
#include <stdlib.h>
#include <stdio.h>



extern MSFilter * ms_ilbc_encoder_new(void);

MSCodecInfo ilbc_info={
	{
		"iLBC codec",
		0,
		MS_FILTER_AUDIO_CODEC,
		ms_ilbc_encoder_new,
		"A speech codec suitable for robust voice communication over IP"
	},
	ms_ilbc_encoder_new,
	ms_ilbc_decoder_new,
	0,	/* not applicable, 2 modes */
	0, /* not applicable, 2 modes */
	15200,
	8000,
	97,
	"iLBC",
	1,
	1,
};


void ms_ilbc_codec_init()
{
	ms_filter_register(MS_FILTER_INFO(&ilbc_info));
}



static MSILBCDecoderClass *ms_ilbc_decoder_class=NULL;

MSFilter * ms_ilbc_decoder_new(void)
{
	MSILBCDecoder *r;
	
	r=g_new(MSILBCDecoder,1);
	ms_ilbc_decoder_init(r);
	if (ms_ilbc_decoder_class==NULL)
	{
		ms_ilbc_decoder_class=g_new(MSILBCDecoderClass,1);
		ms_ilbc_decoder_class_init(ms_ilbc_decoder_class);
	}
	MS_FILTER(r)->klass=MS_FILTER_CLASS(ms_ilbc_decoder_class);
	return(MS_FILTER(r));
}
	

int ms_ilbc_decoder_set_property(MSILBCDecoder *obj, MSFilterProperty prop, char *value)
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
int ms_ilbc_decoder_get_property(MSILBCDecoder *obj, MSFilterProperty prop, char *value)
{
	switch(prop){
		case MS_FILTER_PROPERTY_FMTP:
			if (obj->ms_per_frame==20) strncpy(value,"ptime=20",MS_FILTER_PROPERTY_STRING_MAX_SIZE);
			if (obj->ms_per_frame==30) strncpy(value,"ptime=30",MS_FILTER_PROPERTY_STRING_MAX_SIZE);
		break;
	}
	return 0;
}

void ms_ilbc_decoder_setup(MSILBCDecoder *r) 
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
		g_error("ms_ilbc_decoder_setup: Bad value for ptime (%i)",r->ms_per_frame);
	}
	g_message("Using ilbc decoder with %i ms frames mode.",r->ms_per_frame);
	initDecode(&r->ilbc_dec, r->ms_per_frame /* ms frames */, /* user enhancer */ 0);
}


/* FOR INTERNAL USE*/
void ms_ilbc_decoder_init(MSILBCDecoder *r)
{
	/* default bitrate */
	r->bitrate = 15200;
	r->ms_per_frame = 30;
	r->samples_per_frame = BLOCKL_20MS;
	r->bytes_per_compressed_frame = NO_OF_BYTES_20MS;

	ms_filter_init(MS_FILTER(r));
	MS_FILTER(r)->inqueues=r->q_inputs;
	MS_FILTER(r)->outfifos=r->f_outputs;
	memset(r->q_inputs,0,sizeof(MSFifo*)*MSILBCDECODER_MAX_INPUTS);
	memset(r->f_outputs,0,sizeof(MSFifo*)*MSILBCDECODER_MAX_INPUTS);
}

void ms_ilbc_decoder_class_init(MSILBCDecoderClass *klass)
{
	ms_filter_class_init(MS_FILTER_CLASS(klass));
	ms_filter_class_set_name(MS_FILTER_CLASS(klass),"ILBCDec");
	MS_FILTER_CLASS(klass)->max_qinputs=MSILBCDECODER_MAX_INPUTS;
	MS_FILTER_CLASS(klass)->max_foutputs=MSILBCDECODER_MAX_INPUTS;
	MS_FILTER_CLASS(klass)->w_maxgran= ILBC_MAX_SAMPLES_PER_FRAME*2;
	MS_FILTER_CLASS(klass)->destroy=(MSFilterDestroyFunc)ms_ilbc_decoder_destroy;
	MS_FILTER_CLASS(klass)->set_property=(MSFilterPropertyFunc)ms_ilbc_decoder_set_property;
	MS_FILTER_CLASS(klass)->get_property=(MSFilterPropertyFunc)ms_ilbc_decoder_get_property;
	MS_FILTER_CLASS(klass)->setup=(MSFilterSetupFunc)ms_ilbc_decoder_setup;
	MS_FILTER_CLASS(klass)->process=(MSFilterProcessFunc)ms_ilbc_decoder_process;
	MS_FILTER_CLASS(klass)->info=(MSFilterInfo*)&ilbc_info;
}
	
void ms_ilbc_decoder_process(MSILBCDecoder *r)
{
	MSFifo *fo;
	MSQueue *qi;
	int err1;
	void *dst=NULL;
	float speech[ILBC_MAX_SAMPLES_PER_FRAME];
	MSMessage *m;

	qi=r->q_inputs[0];
	fo=r->f_outputs[0];
	m=ms_queue_get(qi);
	
	ms_fifo_get_write_ptr(fo, r->samples_per_frame*2, &dst);
	if (dst!=NULL){
		if (m->data!=NULL){
			if (m->size<r->bytes_per_compressed_frame) {
				g_warning("Invalid ilbc frame ?");
			}
			iLBC_decode(speech, m->data, &r->ilbc_dec, /* mode */1);
		}else{
			iLBC_decode(speech,NULL, &r->ilbc_dec,0);
		}
		ilbc_write_16bit_samples((gint16*)dst, speech, r->samples_per_frame);
	}
	ms_message_destroy(m);
}

void ms_ilbc_decoder_uninit(MSILBCDecoder *obj)
{
}

void ms_ilbc_decoder_destroy( MSILBCDecoder *obj)
{
	ms_ilbc_decoder_uninit(obj);
	g_free(obj);
}

#endif
