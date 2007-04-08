/*
 * Copyright (C) 2004-2005  Justin Karneges <justin@affinix.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef BOTANTOOLS_H
#define BOTANTOOLS_H

#include <QtGlobal>
#include <botan/mutex.h>
#include <botan/mux_qt.h>
#include <botan/allocate.h>
#include <botan/secmem.h>
#include <botan/bigint.h>
#ifdef Q_OS_UNIX
# include <botan/mmap_mem.h>
#endif

namespace QCA
{
	namespace Botan
	{
		namespace Init
		{
			void set_mutex_type(Mutex*);
			void startup_memory_subsystem();
			void shutdown_memory_subsystem();
		}

		extern int botan_memory_chunk;
		extern int botan_prealloc;
	}
}

#endif
