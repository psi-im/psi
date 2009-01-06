/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#include "qttestutil/testregistry.h"

#include <QtTest/QtTest>

namespace QtTestUtil {

TestRegistry* TestRegistry::getInstance()
{
	static TestRegistry registry;
	return &registry;
}

void TestRegistry::registerTest(QObject* test)
{
	tests_ += test;
}

int TestRegistry::runTests(int argc, char* argv[])
{
	int result = 0;
	foreach(QObject* test, tests_) {
		result |= QTest::qExec(test, argc, argv);
	}
	return result;
}

}
