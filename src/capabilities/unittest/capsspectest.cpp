/**
 * Copyright (C) 2007, Remko Troncon
 */

#include <QObject>
#include <QtTest/QtTest>

#include "qttestutil/qttestutil.h"
#include "capabilities/capsspec.h"

class CapsSpecTest : public QObject
{
		Q_OBJECT

	private slots:
		void testConstructor() {
			CapsSpec c("a","b","c d");

			QVERIFY(c.node() == "a");
			QVERIFY(c.version() == "b");
			QVERIFY(c.extensions() == "c d");
		}

		void testEqualsNotEquals_Equal() {
			CapsSpec c1("a","b","c d");
			CapsSpec c2("a","b","c d");

			QVERIFY(c1 == c2);
			QVERIFY(!(c1 != c2));
		}

		void testEqualsNotEquals_NotEqual() {
			CapsSpec c1("a","b","c d");
			CapsSpec c2("a","e","c d");

			QVERIFY(!(c1 == c2));
			QVERIFY(c1 != c2);
		}

		void testSmallerThan() {
			CapsSpec c1("a","b","c");
			CapsSpec c2("d","e","f");

			QVERIFY(c1 < c2);
			QVERIFY(!(c2 < c1));
		}

		void testSmallerThan_SameNode() {
			CapsSpec c1("a","b","c");
			CapsSpec c2("a","e","f");

			QVERIFY(c1 < c2);
			QVERIFY(!(c2 < c1));
		}

		void testSmallerThan_SameNodeVersion() {
			CapsSpec c1("a","b","c");
			CapsSpec c2("a","b","f");

			QVERIFY(c1 < c2);
			QVERIFY(!(c2 < c1));
		}

		void testSmallerThan_Equals() {
			CapsSpec c1("a","b","c");
			CapsSpec c2("a","b","c");

			QVERIFY(!(c1 < c2));
			QVERIFY(!(c2 < c1));
		}

		void testFlatten() {
			CapsSpec c("a","b","c d");

			CapsSpecs cs = c.flatten();

			QCOMPARE(cs.count(), 3);
			QVERIFY(cs[0] == CapsSpec("a", "b", "b"));
			QVERIFY(cs[1] == CapsSpec("a", "b", "c"));
			QVERIFY(cs[2] == CapsSpec("a", "b", "d"));
		}
};

QTTESTUTIL_REGISTER_TEST(CapsSpecTest);
#include "capsspectest.moc"
