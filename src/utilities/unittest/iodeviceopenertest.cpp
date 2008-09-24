/**
 * Copyright (C) 2007, Remko Troncon
 */

#include <QObject>
#include <QtTest/QtTest>
#include <QList>
#include <QStringList>
#include <QBuffer>

#include "qttestutil/qttestutil.h"
#include "utilities/iodeviceopener.h"

class IODeviceOpenerTest : public QObject
{
		Q_OBJECT

	private slots:
		void testConstructor() {
			QBuffer buffer;

			IODeviceOpener opener(&buffer, QIODevice::WriteOnly);

			QVERIFY(buffer.isOpen());
			QVERIFY(QIODevice::WriteOnly == buffer.openMode());
			QVERIFY(opener.isOpen());
		}

		void testConstructor_Open() {
			QBuffer buffer;
			buffer.open(QIODevice::ReadOnly);

			IODeviceOpener opener(&buffer, QIODevice::ReadOnly);

			QVERIFY(buffer.isOpen());
			QVERIFY(QIODevice::ReadOnly == buffer.openMode());
			QVERIFY(opener.isOpen());
		}

		void testConstructor_OpenInSubsetMode() {
			QBuffer buffer;
			buffer.open(QIODevice::ReadWrite);

			IODeviceOpener opener(&buffer, QIODevice::WriteOnly);

			QVERIFY(buffer.isOpen());
			QVERIFY(QIODevice::ReadWrite == buffer.openMode());
			QVERIFY(opener.isOpen());
		}

		void testConstructor_OpenInWrongMode() {
			QBuffer buffer;
			buffer.open(QIODevice::ReadOnly);

			IODeviceOpener opener(&buffer, QIODevice::WriteOnly);

			QVERIFY(buffer.isOpen());
			QVERIFY(QIODevice::ReadOnly == buffer.openMode());
			QVERIFY(!opener.isOpen());
		}

		void testConstructor_Unopenable() {
			UnopenableBuffer buffer;

			IODeviceOpener opener(&buffer, QIODevice::WriteOnly);

			QVERIFY(!opener.isOpen());
		}

		void testDestructor() {
			QBuffer buffer;

			{
				IODeviceOpener opener(&buffer, QIODevice::WriteOnly);
			}

			QVERIFY(!buffer.isOpen());
		}

		void testDestructor_Open() {
			QBuffer buffer;
			buffer.open(QIODevice::ReadOnly);

			{
				IODeviceOpener opener(&buffer, QIODevice::ReadOnly);
			}

			QVERIFY(buffer.isOpen());
		}

		void testDestructor_OpenInWrongMode() {
			QBuffer buffer;
			buffer.open(QIODevice::ReadOnly);

			{
				IODeviceOpener opener(&buffer, QIODevice::WriteOnly);
			}

			QVERIFY(buffer.isOpen());
			QVERIFY(QIODevice::ReadOnly == buffer.openMode());
		}

	private:
		class UnopenableBuffer : public QBuffer
		{
			public:
				UnopenableBuffer() : QBuffer() { }

				bool open(QIODevice::OpenMode) { return false; } 
		};
};

QTTESTUTIL_REGISTER_TEST(IODeviceOpenerTest);
#include "iodeviceopenertest.moc"
