/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#ifndef DIGESTMD5RESPONSE_H
#define DIGESTMD5RESPONSE_H

#include <QByteArray>
#include <QString>

namespace XMPP {
	class RandomNumberGenerator;

	class DIGESTMD5Response
	{
		public:
			DIGESTMD5Response(
					const QByteArray& challenge, 
					const QString& service, 
					const QString& host, 
					const QString& realm, 
					const QString& user, 
					const QString& authz,  
					const QByteArray& password,
					const RandomNumberGenerator& rand);

			const QByteArray& getValue() const { 
				return value_;
			}

			bool isValid() const { 
				return isValid_;
			}
		
		private:
			bool isValid_;
			QByteArray value_;
	};
}

#endif
