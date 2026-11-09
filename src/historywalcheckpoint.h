/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QWaitCondition>
#include <functional>
#include <utility>

// Owns a separate SQLite connection created and closed on its own thread.
// Checkpointing may call fdatasync() and take seconds on slow HDDs: it must
// never execute as a QTimer callback in the GUI thread.
//
// PASSIVE never waits for readers or writers. If a reader pins the WAL, a
// later checkpoint will copy the remaining frames. Requested checkpoints
// coalesce to one outstanding operation.
class HistoryWalCheckpoint final : public QThread {
public:
    explicit HistoryWalCheckpoint(QString databasePath, std::function<void()> opened = {},
                                  std::function<void()> checkpointed = {})
        : databasePath_(std::move(databasePath)), opened_(std::move(opened)),
          checkpointed_(std::move(checkpointed))
    {
    }

    ~HistoryWalCheckpoint() override
    {
        {
            QMutexLocker lock(&mutex_);
            stopping_ = true;
            pending_ = true; // One final checkpoint on normal shutdown.
            wake_.wakeOne();
        }
        wait();
    }

    void requestCheckpoint()
    {
        QMutexLocker lock(&mutex_);
        if (stopping_)
            return;
        pending_ = true;
        wake_.wakeOne();
    }

protected:
    void run() override
    {
        const auto connectionName = QStringLiteral("history-checkpoint-%1")
                                        .arg(qulonglong(reinterpret_cast<quintptr>(this)), 0, 16);
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            db.setDatabaseName(databasePath_);
            // Never wait for a GUI-held writer. A missed checkpoint is retried.
            db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=0"));
            if (db.open()) {
                if (opened_)
                    opened_();
                for (;;) {
                    bool stopping;
                    {
                        QMutexLocker lock(&mutex_);
                        while (!pending_ && !stopping_)
                            wake_.wait(&mutex_);
                        stopping = stopping_;
                        pending_ = false;
                    }
                    {
                        QSqlQuery checkpoint(db);
                        if (!checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(PASSIVE)")))
                            qWarning() << "History WAL checkpoint failed:" << checkpoint.lastError().text();
                    }
                    if (checkpointed_)
                        checkpointed_();
                    if (stopping)
                        break;
                }
                db.close();
            } else {
                qWarning() << "Cannot open history checkpoint connection:" << db.lastError().text();
            }
        }
        QSqlDatabase::removeDatabase(connectionName);
    }

private:
    QString databasePath_;
    std::function<void()> opened_;
    std::function<void()> checkpointed_;
    QMutex mutex_;
    QWaitCondition wake_;
    bool pending_ = false;
    bool stopping_ = false;
};
