/*
 * Copyright (C) 2008  Remko Troncon
 * Licensed under the GNU GPL.
 * See COPYING for license details.
 */

#include "Certificates/CertificateHelpers.h"

#include <QObject>
#include <QtTest/QtTest>

#include "qttestutil/qttestutil.h"
#include "QtCrypto"

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
