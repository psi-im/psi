/*
 * Copyright (C) 2008  <Your Name>
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

		void testIsValidCertificate_ValidPeerResult() {
			Certificate cert;
			QVERIFY(CertificateHelpers::isValidCertificate(cert, QCA::TLS::Valid, CertificateCollection()));
		}

		void testIsValidCertificate_InvalidPeerResultCertInLibrary() {
			Certificate cert("a");
			CertificateCollection certs;
			certs += Certificate("b");
			certs += Certificate("a");
			QVERIFY(CertificateHelpers::isValidCertificate(cert, QCA::TLS::InvalidCertificate, certs));
		}

		void testIsValidCertificate_InvalidPeerResultCertNotInLibrary() {
			Certificate cert("a");
			CertificateCollection certs;
			certs += Certificate("b");
			QVERIFY(!CertificateHelpers::isValidCertificate(cert, QCA::TLS::InvalidCertificate, certs));
		}
};

QTTESTUTIL_REGISTER_TEST(CertificateHelpersTest);
#include "CertificateHelpersTest.moc"
