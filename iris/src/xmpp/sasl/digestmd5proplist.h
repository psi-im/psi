/*
 * Copyright (C) 2003  Justin Karneges
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

#ifndef DIGESTMD5PROPLIST_H
#define DIGESTMD5PROPLIST_H

#include <QList>
#include <QByteArray>

namespace XMPP {
	struct DIGESTMD5Prop
	{
		QByteArray var, val;
	};

	class DIGESTMD5PropList : public QList<DIGESTMD5Prop>
	{
		public:
			DIGESTMD5PropList();

			void set(const QByteArray &var, const QByteArray &val);
			QByteArray get(const QByteArray &var);
			QByteArray toString() const;
			bool fromString(const QByteArray &str);
		
		private:
			int varCount(const QByteArray &var);
	};
}

#endif
