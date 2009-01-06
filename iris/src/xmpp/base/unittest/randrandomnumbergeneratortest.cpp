/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#include <QObject>
#include <QtTest/QtTest>

#include "qttestutil/qttestutil.h"
#include "xmpp/base/randrandomnumbergenerator.h"

using namespace XMPP;

class RandRandomNumberGeneratorTest : public QObject
{
		Q_OBJECT

	private slots:
		void testGenerateNumber() {
			RandRandomNumberGenerator testling;

			double a = testling.generateNumberBetween(0.0,100.0);
			double b = testling.generateNumberBetween(0.0,100.0);

			QVERIFY(a != b);
		}
 };

QTTESTUTIL_REGISTER_TEST(RandRandomNumberGeneratorTest);
#include "randrandomnumbergeneratortest.moc"
