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
        QVERIFY(PsiMam::mayFetchArchive(true, false, true));
        QVERIFY(!PsiMam::mayFetchArchive(true, true, true));
    }
    void cacheRequiresWholeStablePage()
    {
        QVERIFY(PsiMam::mayReuseArchivePage(false,true,true,true,40,40,false));
        QVERIFY(!PsiMam::mayReuseArchivePage(false,true,true,true,39,40,false));
        QVERIFY(PsiMam::mayReuseArchivePage(false,true,true,true,3,40,true));
        QVERIFY(PsiMam::mayReuseArchivePage(false,true,true,true,0,40,true));
        QVERIFY(!PsiMam::mayReuseArchivePage(false,true,true,false,40,40,true));
        QVERIFY(!PsiMam::mayReuseArchivePage(false,true,false,true,40,40,true));
        QVERIFY(!PsiMam::mayReuseArchivePage(true,true,true,true,40,40,true));
        QVERIFY(PsiMam::mayReuseArchivePage(true,false,true,true,40,40,true));
    }
    void continuationKeepsItsArchiveCursor()
    {
        // A fetch with an established server UID is never the initial mixed
        // SQLite/MAM query. This also covers resumption after reconnect.
        QVERIFY(PsiMam::mayFetchArchive(false, false, false));
        QVERIFY(PsiMam::mayFetchArchive(false, false, true));
        QVERIFY(!PsiMam::mayFetchArchive(false, true, true));
    }
};

QTEST_MAIN(MamHistorySourcePolicyTest)
#include "MamHistorySourcePolicyTest.moc"
