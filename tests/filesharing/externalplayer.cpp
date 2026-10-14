#include "desktoputil.h"

#include <QApplication>
#include <QFile>
#include <QInputDialog>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrl>

class ExternalPlayerTest : public QObject {
    Q_OBJECT
private slots:
    void externalPlayerReceivesOneArgument()
    {
#ifdef Q_OS_UNIX
        QTemporaryDir dir(QDir::currentPath() + "/player-test-XXXXXX");
        QVERIFY(dir.isValid());
        QString player = dir.path() + "/fake player";
        QFile   script(player);
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nprintf '%s\\n' \"$#\" \"$1\" > \"$0.args\"\n");
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QUrl url("http://127.0.0.1:12345/psi/account/test/sharedfile/sha-1+abcdef?x=a%20b&y=$value");
        QVERIFY(DesktopUtil::openMediaUrl(url, player));
        QFile args(player + ".args");
        QTRY_VERIFY(args.exists() && args.size() > 0);
        QVERIFY(args.open(QIODevice::ReadOnly));
        QCOMPARE(args.readAll(), QByteArray("1\n") + url.toEncoded() + '\n');
#else
        QSKIP("The executable fixture requires a POSIX shell.");
#endif
    }
    void cancellingPlayerSelection()
    {
        QString player;
        QTimer::singleShot(0, [] {
            if (auto dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget()))
                dialog->reject();
        });
        QVERIFY(!DesktopUtil::openMediaUrl(QUrl("http://127.0.0.1:12345/media"), player));
        QVERIFY(player.isEmpty());
    }
};
QTEST_MAIN(ExternalPlayerTest)
#include "externalplayer.moc"
