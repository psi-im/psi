/*
 * Copyright (C) 2008  <Your Name>
 * See COPYING for license details.
 */

#include <QObject>
#include <QtTest/QtTest>

#include "qttestutil/qttestutil.h"

class MyClassTest : public QObject
{
     Q_OBJECT
	
	private slots:
		void initTestCase() {
		}

		void cleanupTestCase() {
		}

		void testMyMethod() {
			//QCOMPARE(foo, bar);
			//QVERIFY(baz);
		}
};

QTTESTUTIL_REGISTER_TEST(MyClassTest);
#include "myclasstest.moc"
