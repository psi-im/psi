/*
 * Copyright (C) 2008  Remko Troncon
 * Licensed under the GNU GPL.
 * See COPYING for license details.
 */

#include <QObject>
#include <QtTest/QtTest>

#include "qttestutil/qttestutil.h"

#include "QtCrypto"
#include "Certificates/CertificateHelpers.h"

using namespace QCA;

class CertificateHelpersTest : public QObject
{
		 Q_OBJECT
	
	private slots:
		void initTestCase() {
		}

		void cleanupTestCase() {
		}
};

QTTESTUTIL_REGISTER_TEST(CertificateHelpersTest);
#include "CertificateHelpersTest.moc"
