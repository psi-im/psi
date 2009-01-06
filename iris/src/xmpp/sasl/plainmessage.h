/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#ifndef PLAINMESSAGE_H
#define PLAINMESSAGE_H

#include <QByteArray>
#include <QString>

namespace XMPP {
	class PLAINMessage
	{
		public:
			PLAINMessage(const QString& authzid, const QString& authcid, const QByteArray& password);

			const QByteArray& getValue() {
				return value_;
			}

		private:
			QByteArray value_;
	};
}

#endif
