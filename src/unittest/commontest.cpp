/**
 * Copyright (C) 2007, Remko Troncon
 */

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include "common.h"

int versionStringToInt(const char* version);

// -----------------------------------------------------------------------------

class CommonTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(CommonTest);

	CPPUNIT_TEST(testVersionStringToInt);
	CPPUNIT_TEST(testVersionStringToInt_TooManyParts);
	CPPUNIT_TEST(testVersionStringToInt_TooFewParts);
	CPPUNIT_TEST(testVersionStringToInt_NonNumericPart);
	CPPUNIT_TEST(testVersionStringToInt_TooBigPart);
	CPPUNIT_TEST(testVersionStringToInt_TooSmallPart);

	CPPUNIT_TEST_SUITE_END();

public:
	CommonTest();

	void testVersionStringToInt();
	void testVersionStringToInt_TooManyParts();
	void testVersionStringToInt_TooFewParts();
	void testVersionStringToInt_NonNumericPart();
	void testVersionStringToInt_TooBigPart();
	void testVersionStringToInt_TooSmallPart();
};

CPPUNIT_TEST_SUITE_REGISTRATION(CommonTest);

CommonTest::CommonTest()
{
}

void CommonTest::testVersionStringToInt()
{
  CPPUNIT_ASSERT_EQUAL(0x00040300, versionStringToInt("4.3.0"));
}

void CommonTest::testVersionStringToInt_TooManyParts()
{
  CPPUNIT_ASSERT_EQUAL(0, versionStringToInt("4.3.0.1"));
}

void CommonTest::testVersionStringToInt_TooFewParts()
{
  CPPUNIT_ASSERT_EQUAL(0, versionStringToInt("4.3"));
}

void CommonTest::testVersionStringToInt_NonNumericPart()
{
  CPPUNIT_ASSERT_EQUAL(0, versionStringToInt("4.A.3"));
}

void CommonTest::testVersionStringToInt_TooBigPart()
{
  CPPUNIT_ASSERT_EQUAL(0, versionStringToInt("4.256.4"));
}

void CommonTest::testVersionStringToInt_TooSmallPart()
{
  CPPUNIT_ASSERT_EQUAL(0, versionStringToInt("4.-1.4"));
}

