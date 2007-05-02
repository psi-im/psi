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

#ifndef DIRWATCH_P_H
#define DIRWATCH_P_H

#include "qca_support.h"

namespace QCA
{
	class DirWatchPlatform : public QObject
	{
		Q_OBJECT
	public:
		DirWatchPlatform(QObject *parent = 0);
		~DirWatchPlatform();

		static bool isSupported();
		bool init();
		int addDir(const QString &);
		void removeDir(int);

	Q_SIGNALS:
		void dirChanged(int);

	private:
		class Private;
		friend class Private;
		Private *d;
	};
}

#endif
