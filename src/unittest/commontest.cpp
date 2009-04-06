/**
 * Copyright (C) 2007, Remko Troncon
 */

#include <QObject>
#include <QtTest/QtTest>

#include "qttestutil/qttestutil.h"
#include "common.h"

int versionStringToInt(const char* version);

class CommonTest : public QObject
{
		Q_OBJECT

	private slots:
		void testVersionStringToInt() {
			QCOMPARE(versionStringToInt("4.3.0"), 0x00040300);
		}

		void testVersionStringToInt_TooManyParts() {
			QCOMPARE(versionStringToInt("4.3.0.1"), 0);
		}

		void testVersionStringToInt_TooFewParts() {
			QCOMPARE(versionStringToInt("4.3"), 0);
		}

		void testVersionStringToInt_NonNumericPart() {
			QCOMPARE(versionStringToInt("4.A.3"), 0);
		}

		void testVersionStringToInt_TooBigPart() {
			QCOMPARE(versionStringToInt("4.256.4"), 0);
		}

		void testVersionStringToInt_TooSmallPart() {
			QCOMPARE(versionStringToInt("4.-1.4"), 0);
		}
};

QTTESTUTIL_REGISTER_TEST(CommonTest);

