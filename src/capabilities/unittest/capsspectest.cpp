/**
 * Copyright (C) 2007, Remko Troncon
 */

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include "capsspec.h"

// -----------------------------------------------------------------------------

class CapsSpecTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(CapsSpecTest);
	CPPUNIT_TEST(testConstructor);

	CPPUNIT_TEST(testEqualsNotEquals_Equal);
	CPPUNIT_TEST(testEqualsNotEquals_NotEqual);

	CPPUNIT_TEST(testSmallerThan);
	CPPUNIT_TEST(testSmallerThan_SameNode);
	CPPUNIT_TEST(testSmallerThan_SameNodeVersion);
	CPPUNIT_TEST(testSmallerThan_Equals);

	CPPUNIT_TEST(testFlatten);

	CPPUNIT_TEST_SUITE_END();

public:
	CapsSpecTest();

	void testConstructor();
	void testFlatten();
	void testEqualsNotEquals_Equal();
	void testEqualsNotEquals_NotEqual();
	void testSmallerThan();
	void testSmallerThan_SameNode();
	void testSmallerThan_SameNodeVersion();
	void testSmallerThan_Equals();
};

CPPUNIT_TEST_SUITE_REGISTRATION(CapsSpecTest);

// -----------------------------------------------------------------------------

CapsSpecTest::CapsSpecTest()
{
}

void CapsSpecTest::testConstructor()
{
	CapsSpec c("a","b","c d");

	CPPUNIT_ASSERT(c.node() == "a");
	CPPUNIT_ASSERT(c.version() == "b");
	CPPUNIT_ASSERT(c.extensions() == "c d");
}

void CapsSpecTest::testEqualsNotEquals_Equal()
{
	CapsSpec c1("a","b","c d");
	CapsSpec c2("a","b","c d");

	CPPUNIT_ASSERT(c1 == c2);
	CPPUNIT_ASSERT(!(c1 != c2));
}

void CapsSpecTest::testEqualsNotEquals_NotEqual()
{
	CapsSpec c1("a","b","c d");
	CapsSpec c2("a","e","c d");

	CPPUNIT_ASSERT(!(c1 == c2));
	CPPUNIT_ASSERT(c1 != c2);
}

void CapsSpecTest::testSmallerThan()
{
	CapsSpec c1("a","b","c");
	CapsSpec c2("d","e","f");

	CPPUNIT_ASSERT(c1 < c2);
	CPPUNIT_ASSERT(!(c2 < c1));
}

void CapsSpecTest::testSmallerThan_SameNode()
{
	CapsSpec c1("a","b","c");
	CapsSpec c2("a","e","f");

	CPPUNIT_ASSERT(c1 < c2);
	CPPUNIT_ASSERT(!(c2 < c1));
}

void CapsSpecTest::testSmallerThan_SameNodeVersion()
{
	CapsSpec c1("a","b","c");
	CapsSpec c2("a","b","f");

	CPPUNIT_ASSERT(c1 < c2);
	CPPUNIT_ASSERT(!(c2 < c1));
}

void CapsSpecTest::testSmallerThan_Equals()
{
	CapsSpec c1("a","b","c");
	CapsSpec c2("a","b","c");

	CPPUNIT_ASSERT(!(c1 < c2));
	CPPUNIT_ASSERT(!(c2 < c1));
}

void CapsSpecTest::testFlatten()
{
	CapsSpec c("a","b","c d");

	CapsSpecs cs = c.flatten();

	CPPUNIT_ASSERT_EQUAL(3, cs.count());
	CPPUNIT_ASSERT(cs[0] == CapsSpec("a", "b", "b"));
	CPPUNIT_ASSERT(cs[1] == CapsSpec("a", "b", "c"));
	CPPUNIT_ASSERT(cs[2] == CapsSpec("a", "b", "d"));
}

