/**
 * Copyright (C) 2007, Remko Troncon
 */

#include <QList>
#include <QStringList>
#include <QBuffer>

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include "iodeviceopener.h"

// -----------------------------------------------------------------------------

class IODeviceOpenerTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(IODeviceOpenerTest);

	CPPUNIT_TEST(testConstructor);
	CPPUNIT_TEST(testConstructor_Open);
	CPPUNIT_TEST(testConstructor_OpenInSubsetMode);
	CPPUNIT_TEST(testConstructor_OpenInWrongMode);
	CPPUNIT_TEST(testConstructor_Unopenable);

	CPPUNIT_TEST(testDestructor);
	CPPUNIT_TEST(testDestructor_Open);
	CPPUNIT_TEST(testDestructor_OpenInWrongMode);

	CPPUNIT_TEST_SUITE_END();

public:
	IODeviceOpenerTest();

	void testConstructor();
	void testConstructor_Open();
	void testConstructor_OpenInSubsetMode();
	void testConstructor_OpenInWrongMode();
	void testConstructor_Unopenable();

	void testDestructor();
	void testDestructor_Open();
	void testDestructor_OpenInWrongMode();
};

CPPUNIT_TEST_SUITE_REGISTRATION(IODeviceOpenerTest);

// -----------------------------------------------------------------------------

class UnopenableBuffer : public QBuffer
{
public:
	UnopenableBuffer() : QBuffer() { }

	bool open(QIODevice::OpenMode) { return false; } 
};

// -----------------------------------------------------------------------------

IODeviceOpenerTest::IODeviceOpenerTest()
{
}

void IODeviceOpenerTest::testConstructor()
{
	QBuffer buffer;

	IODeviceOpener opener(&buffer, QIODevice::WriteOnly);

	CPPUNIT_ASSERT(buffer.isOpen());
	CPPUNIT_ASSERT(QIODevice::WriteOnly == buffer.openMode());
	CPPUNIT_ASSERT(opener.isOpen());
}

void IODeviceOpenerTest::testConstructor_Open()
{
	QBuffer buffer;
	buffer.open(QIODevice::ReadOnly);

	IODeviceOpener opener(&buffer, QIODevice::ReadOnly);

	CPPUNIT_ASSERT(buffer.isOpen());
	CPPUNIT_ASSERT(QIODevice::ReadOnly == buffer.openMode());
	CPPUNIT_ASSERT(opener.isOpen());
}

void IODeviceOpenerTest::testConstructor_OpenInSubsetMode()
{
	QBuffer buffer;
	buffer.open(QIODevice::ReadWrite);

	IODeviceOpener opener(&buffer, QIODevice::WriteOnly);

	CPPUNIT_ASSERT(buffer.isOpen());
	CPPUNIT_ASSERT(QIODevice::ReadWrite == buffer.openMode());
	CPPUNIT_ASSERT(opener.isOpen());
}

void IODeviceOpenerTest::testConstructor_OpenInWrongMode()
{
	QBuffer buffer;
	buffer.open(QIODevice::ReadOnly);

	IODeviceOpener opener(&buffer, QIODevice::WriteOnly);

	CPPUNIT_ASSERT(buffer.isOpen());
	CPPUNIT_ASSERT(QIODevice::ReadOnly == buffer.openMode());
	CPPUNIT_ASSERT(!opener.isOpen());
}

void IODeviceOpenerTest::testConstructor_Unopenable()
{
	UnopenableBuffer buffer;

	IODeviceOpener opener(&buffer, QIODevice::WriteOnly);

	CPPUNIT_ASSERT(!opener.isOpen());
}

void IODeviceOpenerTest::testDestructor()
{
	QBuffer buffer;

	{
		IODeviceOpener opener(&buffer, QIODevice::WriteOnly);
	}

	CPPUNIT_ASSERT(!buffer.isOpen());
}

void IODeviceOpenerTest::testDestructor_Open()
{
	QBuffer buffer;
	buffer.open(QIODevice::ReadOnly);

	{
		IODeviceOpener opener(&buffer, QIODevice::ReadOnly);
	}

	CPPUNIT_ASSERT(buffer.isOpen());
}

void IODeviceOpenerTest::testDestructor_OpenInWrongMode()
{
	QBuffer buffer;
	buffer.open(QIODevice::ReadOnly);

	{
		IODeviceOpener opener(&buffer, QIODevice::WriteOnly);
	}

	CPPUNIT_ASSERT(buffer.isOpen());
	CPPUNIT_ASSERT(QIODevice::ReadOnly == buffer.openMode());
}
