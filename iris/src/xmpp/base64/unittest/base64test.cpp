/*
 * Copyright (C) 2008  <Your Name>
 * See COPYING for license details.
 */

#include <QObject>
#include <QtTest/QtTest>

#include "qttestutil/qttestutil.h"
#include "xmpp/base64/base64.h"

using namespace XMPP;

class Base64Test : public QObject
{
     Q_OBJECT
	
	private slots:
		void testEncode() {
			QString result = Base64::encode("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890");
			QCOMPARE(result, QString("QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVphYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ejEyMzQ1Njc4OTA="));
		}

		void testDecode() {
			QString result = Base64::decode("QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVphYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ejEyMzQ1Njc4OTA=");
			QCOMPARE(result, QString("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890"));
		}
};

QTTESTUTIL_REGISTER_TEST(Base64Test);
#include "base64test.moc"
