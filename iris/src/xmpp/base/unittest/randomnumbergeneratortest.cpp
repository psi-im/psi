/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#include <QObject>
#include <QtTest/QtTest>

#include "qttestutil/qttestutil.h"
#include "xmpp/base/randomnumbergenerator.h"

using namespace XMPP;

class RandomNumberGeneratorTest : public QObject
{
     Q_OBJECT
	
	private:
		class DummyRandomNumberGenerator : public RandomNumberGenerator
		{
			public:
				DummyRandomNumberGenerator(double value, double maximum) : value_(value), maximum_(maximum) {}

				double generateNumber() const { return value_; }
				double getMaximumGeneratedNumber() const { return maximum_; }

			private:
				double value_;
				double maximum_;
		};

	private slots:
		void testGenerateNumberBetween() {
			DummyRandomNumberGenerator testling(5,10);
			QCOMPARE(75.0, testling.generateNumberBetween(50.0, 100.0));
		}

		void testGenerateNumberBetween_Minimum() {
			DummyRandomNumberGenerator testling(0,10);
			QCOMPARE(0.0, testling.generateNumberBetween(0.0, 100.0));
		}

		void testGenerateNumberBetween_Maximum() {
			DummyRandomNumberGenerator testling(10,10);
			QCOMPARE(100.0, testling.generateNumberBetween(0.0, 100.0));
		}
};

QTTESTUTIL_REGISTER_TEST(RandomNumberGeneratorTest);
#include "randomnumbergeneratortest.moc"
