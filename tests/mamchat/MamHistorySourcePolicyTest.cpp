/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MamHistorySourcePolicy.h"

#include <QtTest/QtTest>

class MamHistorySourcePolicyTest final : public QObject {
    Q_OBJECT
private slots:
    void initialSourceSelection()
    {
        QVERIFY(PsiMam::mayFetchArchive(true, false, false));
        QVERIFY(!PsiMam::mayFetchArchive(true, true, false));
        QVERIFY(!PsiMam::mayFetchArchive(true, false, true));
        QVERIFY(!PsiMam::mayFetchArchive(true, true, true));
    }
    void continuationKeepsItsArchiveCursor()
    {
        // A fetch with an established server UID is never the initial mixed
        // SQLite/MAM query. This also covers resumption after reconnect.
        QVERIFY(PsiMam::mayFetchArchive(false, false, false));
        QVERIFY(PsiMam::mayFetchArchive(false, false, true));
        QVERIFY(PsiMam::mayFetchArchive(false, true, true));
    }
};

QTEST_MAIN(MamHistorySourcePolicyTest)
#include "MamHistorySourcePolicyTest.moc"
