/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

// FIXME: Complete this

#include <QtTest/QtTest>
#include <QObject>

#include "qttestutil/qttestutil.h"
#include "xmpp/jid/jid.h"

using namespace XMPP;

class JidTest : public QObject
{
		Q_OBJECT

	private slots:
		void testConstructorWithString() {
			Jid testling("foo@bar/baz");

			QCOMPARE(testling.node(), QString("foo"));
			QCOMPARE(testling.domain(), QString("bar"));
			QCOMPARE(testling.resource(), QString("baz"));
		}
};

QTTESTUTIL_REGISTER_TEST(JidTest);
#include "jidtest.moc"
