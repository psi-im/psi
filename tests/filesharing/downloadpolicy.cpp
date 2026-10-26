/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "mediadownloadpolicy.h"

#include <QtTest>

class MediaDownloadPolicyTest final : public QObject {
    Q_OBJECT

    static MediaDownloadPolicy::Settings settings()
    {
        MediaDownloadPolicy::Settings settings;
        settings.chatLimit      = 5 * 1024 * 1024;
        settings.groupchatLimit = 1024 * 1024;
        return settings;
    }

private slots:
    void chatsDownloadOnlyFromTrustedSenders()
    {
        QCOMPARE(MediaDownloadPolicy::limit(settings(), false, true), quint64(5 * 1024 * 1024));
        QCOMPARE(MediaDownloadPolicy::limit(settings(), false, false), quint64(0));
    }

    void groupchatsDownloadFromEveryone()
    {
        QCOMPARE(MediaDownloadPolicy::limit(settings(), true, false), quint64(1024 * 1024));
        QCOMPARE(MediaDownloadPolicy::limit(settings(), true, true), quint64(1024 * 1024));
    }

    void nothingDownloadsWithoutPreviews()
    {
        auto withoutPreviews     = settings();
        withoutPreviews.previews = false;
        QCOMPARE(MediaDownloadPolicy::limit(withoutPreviews, false, true), quint64(0));
        QCOMPARE(MediaDownloadPolicy::limit(withoutPreviews, true, true), quint64(0));
    }

    void choicesStartWithNeverAndGrow()
    {
        const auto choices = MediaDownloadPolicy::choices();
        QVERIFY(choices.size() > 1);
        QCOMPARE(choices.first(), quint64(0));
        for (int i = 1; i < choices.size(); ++i)
            QVERIFY(choices.at(i) > choices.at(i - 1));
        // The defaults in options/default.xml are among them.
        QVERIFY(choices.contains(5 * 1024 * 1024));
        QVERIFY(choices.contains(1024 * 1024));
    }
};

QTEST_GUILESS_MAIN(MediaDownloadPolicyTest)
#include "downloadpolicy.moc"
