// SPDX-License-Identifier: GPL-2.0-or-later
#include "accountconnectionguard.h"
#include <QEventLoop>
#include <QTest>
#include <QTimer>

class Stream : public QObject {
    Q_OBJECT
signals:
    void needAuthParams(bool, bool, bool);
    void error(int);
    void closed();
};

class Account : public QObject {
public:
    quint64 generation   = 0;
    int     authRequests = 0;
    int     errors       = 0;
    int     closes       = 0;
    void    needAuthParams(bool user, bool pass, bool realm)
    {
        if (user && pass && !realm)
            ++authRequests;
    }
    void error(int) { ++errors; }
    void closed() { ++closes; }

    void attach(Stream *stream)
    {
        ++generation;
        const AccountConnectionGuard<Stream> guard(stream, generation);
        connect(stream, &Stream::needAuthParams, this, guard.callback(this, &Account::needAuthParams));
        connect(stream, &Stream::error, this, guard.callback(this, &Account::error), Qt::QueuedConnection);
        connect(stream, &Stream::closed, this, guard.callback(this, &Account::closed), Qt::QueuedConnection);
    }
};

class ConnectionGuardTest : public QObject {
    Q_OBJECT
private slots:
    void staleSignalsCannotReachNewAttempt()
    {
        Account account;
        Stream  oldStream, currentStream;
        account.attach(&oldStream);
        emit oldStream.error(1);
        emit oldStream.closed();
        account.attach(&currentStream);
        // Match the captured crash: the previous source is still alive and
        // requests credentials after a new stream has become current.
        emit oldStream.needAuthParams(true, true, false);
        QCoreApplication::processEvents();
        QCOMPARE(account.authRequests, 0);
        QCOMPARE(account.errors, 0);
        QCOMPARE(account.closes, 0);
        emit currentStream.needAuthParams(true, true, false);
        emit currentStream.error(2);
        emit currentStream.closed();
        QCoreApplication::processEvents();
        QCOMPARE(account.authRequests, 1);
        QCOMPARE(account.errors, 1);
        QCOMPARE(account.closes, 1);
    }

    void queuedDeliveryAfterSourceDestruction()
    {
        Account account;
        auto    stream = new Stream;
        account.attach(stream);
        emit stream->error(1);
        emit stream->closed();
        delete stream;
        QCoreApplication::processEvents();
        QCOMPARE(account.errors, 0);
        QCOMPARE(account.closes, 0);
    }

    void delayedCredentialsAndNestedEventLoop()
    {
        Account account;
        Stream  oldStream, currentStream;
        account.attach(&oldStream);
        const AccountConnectionGuard<Stream> lookup(&oldStream, account.generation);
        bool                                 credentialsApplied = false;
        QEventLoop                           loop;
        QTimer::singleShot(0, &account, [&] {
            account.attach(&currentStream);
            loop.quit();
        });
        loop.exec(); // A keyring unlock or password dialog allows reconnect.
        if (lookup.isCurrent())
            credentialsApplied = true;
        QVERIFY(!credentialsApplied);
        const AccountConnectionGuard<Stream> currentLookup(&currentStream, account.generation);
        QVERIFY(currentLookup.isCurrent());
        ++account.generation; // cleanup invalidates callbacks before teardown
        QVERIFY(!currentLookup.isCurrent());
    }

    void receiverDestructionCancelsDelivery()
    {
        Stream stream;
        auto   account = new Account;
        account->attach(&stream);
        emit stream.error(1);
        delete account; // destroys the counter referenced by the guard as well
        QCoreApplication::processEvents();
    }
};

QTEST_GUILESS_MAIN(ConnectionGuardTest)
#include "connectionguard.moc"
