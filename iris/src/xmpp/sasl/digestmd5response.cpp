/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#include "xmpp/sasl/digestmd5response.h"

#include <QByteArray>
#include <QtCrypto>
#include <QtDebug>

#include "xmpp/sasl/digestmd5proplist.h"
#include "xmpp/base/randomnumbergenerator.h"
#include "xmpp/base64/base64.h"

namespace XMPP {

DIGESTMD5Response::DIGESTMD5Response(const QByteArray& challenge, const QString& service, const QString& host, const QString& arealm, const QString& user, const QString& authz,  const QByteArray& password, const RandomNumberGenerator& rand) : isValid_(true)
{
	QString realm = arealm;

	// get props
	DIGESTMD5PropList in;
	if(!in.fromString(challenge)) {
		isValid_ = false;
		return;
	}
	//qDebug() << (QString("simplesasl.cpp: IN: %1").arg(QString(in.toString())));

	// make a cnonce
	QByteArray a;
	a.resize(32);
	for(int n = 0; n < (int)a.size(); ++n) {
		a[n] = (char) rand.generateNumberBetween(0, 255); 
	}
	QByteArray cnonce = Base64::encode(a).toLatin1();

	// make other variables
	if (realm.isEmpty()) {
		realm = QString::fromUtf8(in.get("realm"));
	}
	QByteArray nonce = in.get("nonce");
	QByteArray nc = "00000001";
	QByteArray uri = service.toUtf8() + '/' + host.toUtf8();
	QByteArray qop = "auth";

	// build 'response'
	QByteArray X = user.toUtf8() + ':' + realm.toUtf8() + ':' + password;
	QByteArray Y = QCA::Hash("md5").hash(X).toByteArray();
	QByteArray tmp = ':' + nonce + ':' + cnonce;
	if (!authz.isEmpty())
		tmp += ':' + authz.toUtf8();
	//qDebug() << (QString(tmp));

	QByteArray A1(Y + tmp);
	QByteArray A2 = QByteArray("AUTHENTICATE:") + uri;
	QByteArray HA1 = QCA::Hash("md5").hashToString(A1).toLatin1();
	QByteArray HA2 = QCA::Hash("md5").hashToString(A2).toLatin1();
	QByteArray KD = HA1 + ':' + nonce + ':' + nc + ':' + cnonce + ':' + qop + ':' + HA2;
	QByteArray Z = QCA::Hash("md5").hashToString(KD).toLatin1();

	//qDebug() << (QString("simplesasl.cpp: A1 = %1").arg(QString(A1)).toAscii());
	//qDebug() << (QString("simplesasl.cpp: A2 = %1").arg(QString(A2)).toAscii());
	//qDebug() << (QString("simplesasl.cpp: KD = %1").arg(QString(KD)).toAscii());

	// build output
	DIGESTMD5PropList out;
	out.set("username", user.toUtf8());
	if (!realm.isEmpty())
		out.set("realm", realm.toUtf8());
	out.set("nonce", nonce);
	out.set("cnonce", cnonce);
	out.set("nc", nc);
	//out.set("serv-type", service.toUtf8());
	//out.set("host", host.toUtf8());
	out.set("digest-uri", uri);
	out.set("qop", qop);
	out.set("response", Z);
	out.set("charset", "utf-8");
	if (!authz.isEmpty())
		out.set("authzid", authz.toUtf8());
	value_  = out.toString();
}

}
