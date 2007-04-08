/*
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "sprocess.h"

#ifdef Q_OS_UNIX
# include <unistd.h>
#endif

namespace gpgQCAPlugin {

//----------------------------------------------------------------------------
// SProcess
//----------------------------------------------------------------------------
SProcess::SProcess(QObject *parent)
:QProcess(parent)
{
}

SProcess::~SProcess()
{
}

#ifdef Q_OS_UNIX
void SProcess::setClosePipeList(const QList<int> &list)
{
	pipeList = list;
}

void SProcess::setupChildProcess()
{
	// close all pipes
	for(int n = 0; n < pipeList.count(); ++n)
		::close(pipeList[n]);
}
#endif

}
