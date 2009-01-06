/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#include <QObject>
#include <QtTest/QtTest>

#include "xmpp/sasl/plainmessage.h"
#include "qttestutil/qttestutil.h"

using namespace XMPP;

class PlainMessageTest : public QObject
{
		Q_OBJECT

	private slots:
		void testConstructor_WithoutAuthzID() {
			PLAINMessage message("", QString("user"), "pass");
			QCOMPARE(message.getValue(), QByteArray("\0user\0pass", 10));
		}

		void testConstructor_WithAuthzID() {
			PLAINMessage message(QString("authz"), QString("user"), "pass");
			QCOMPARE(message.getValue(), QByteArray("authz\0user\0pass", 15));
		}

		void testConstructor_WithNonASCIICharacters() {
			PLAINMessage message(QString("authz") + QChar(0x03A8) /* psi */, QString("user") + QChar(0x03A8) /* psi */, "pass");
			QCOMPARE(message.getValue(), QByteArray("authz\xCE\xA8\0user\xCE\xA8\0pass", 19));
		}
};

QTTESTUTIL_REGISTER_TEST(PlainMessageTest);
#include "plainmessagetest.moc"
