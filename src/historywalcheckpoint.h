/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDebug>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <utility>

// The worker owns its SQLite connection exclusively. No database handle,
// mutable state or mutex crosses the thread boundary. Each slot executes in
// the worker's event loop; requests are serialized by queued connections.
class HistoryWalCheckpointWorker final : public QObject {
    Q_OBJECT
public:
    explicit HistoryWalCheckpointWorker(QString databasePath) : databasePath_(std::move(databasePath)) { }

public slots:
    void open()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        connectionName_ = QStringLiteral("history-checkpoint-%1")
                              .arg(qulonglong(reinterpret_cast<quintptr>(this)), 0, 16);
        db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
        db_.setDatabaseName(databasePath_);
        // PASSIVE checkpoints should never queue behind a foreground writer.
        db_.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=0"));
        if (!db_.open()) {
            qWarning() << "Cannot open history checkpoint connection:" << db_.lastError().text();
            return;
        }
        emit opened();
    }

    void checkpoint()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (db_.isOpen()) {
            QSqlQuery query(db_);
            if (!query.exec(QStringLiteral("PRAGMA wal_checkpoint(PASSIVE)")))
                qWarning() << "History WAL checkpoint failed:" << query.lastError().text();
        }
        emit completed();
    }

    void close()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        // A final checkpoint is ordered after all already queued checkpoints.
        if (db_.isOpen())
            checkpoint();
        db_.close();
        db_ = QSqlDatabase();
        if (!connectionName_.isEmpty())
            QSqlDatabase::removeDatabase(connectionName_);
        emit stopped();
    }

signals:
    void opened();
    void completed();
    void stopped();

private:
    QString databasePath_;
    QString connectionName_;
    QSqlDatabase db_; // Accessed only on the worker thread.
};

// GUI-side controller. Pending/coalescing state lives entirely in the GUI
// thread. The only cross-thread communication is queued signals/slots.
// This is also the ownership pattern for the future fully asynchronous EDB.
class HistoryWalCheckpoint final : public QObject {
    Q_OBJECT
public:
    explicit HistoryWalCheckpoint(QString databasePath)
    {
        worker_ = new HistoryWalCheckpointWorker(std::move(databasePath));
        worker_->moveToThread(&thread_);
        connect(&thread_, &QThread::started, worker_, &HistoryWalCheckpointWorker::open);
        connect(this, &HistoryWalCheckpoint::checkpointRequested,
                worker_, &HistoryWalCheckpointWorker::checkpoint, Qt::QueuedConnection);
        connect(this, &HistoryWalCheckpoint::stopRequested,
                worker_, &HistoryWalCheckpointWorker::close, Qt::QueuedConnection);
        connect(worker_, &HistoryWalCheckpointWorker::opened, this,
                &HistoryWalCheckpoint::ready, Qt::QueuedConnection);
        connect(worker_, &HistoryWalCheckpointWorker::completed, this,
                [this] {
                    pending_ = false;
                    emit checkpointCompleted();
                }, Qt::QueuedConnection);
        connect(worker_, &HistoryWalCheckpointWorker::stopped, &thread_, &QThread::quit,
                Qt::DirectConnection);
        connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    }

    ~HistoryWalCheckpoint() override
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (thread_.isRunning()) {
            emit stopRequested();
            thread_.wait(); // Only on final shutdown; no wait in the GUI event loop.
        } else {
            delete worker_;
        }
    }

    void start() { thread_.start(QThread::LowPriority); }

    void requestCheckpoint()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (pending_ || !thread_.isRunning())
            return;
        pending_ = true;
        emit checkpointRequested();
    }

signals:
    void ready();
    void checkpointCompleted();
    void checkpointRequested();
    void stopRequested();

private:
    QThread thread_;
    HistoryWalCheckpointWorker *worker_ = nullptr;
    bool pending_ = false; // GUI thread only.
};
