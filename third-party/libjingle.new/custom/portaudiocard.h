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
/* An implementation of SndCard : the OssCard */

#ifndef PORTAUDIO_CARD_H
#define PORTAUDIO_CARD_H

#include "sndcard.h"

typedef struct _PortAudioCard
{
	SndCard parent;
	PortAudioStream* stream;
	char *out_buffer, *out_buffer_read, *out_buffer_write, *out_buffer_end;
	char *in_buffer, *in_buffer_read, *in_buffer_write, *in_buffer_end;
} PortAudioCard;

gint portaudio_card_manager_init(SndCardManager *manager, gint tabindex);

#endif
