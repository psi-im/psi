/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "historyarchive.h"

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlRecord>
#include <QString>
#include <utility>

// Only immutable value objects cross the EDB reader's queued connections.
// No PsiEvent, PsiAccount or SQLite connection ever crosses into the worker.
struct HistoryReadRequest {
    enum Type { Get, Find, LocalPage, DisplayedCursor };
    int id = 0;
    Type type = Get;
    QString account, peer, text;
    QDateTime date;
    qint64 before = 0;
    int direction = 0, start = 0, limit = 40;
};
Q_DECLARE_METATYPE(HistoryReadRequest)

struct HistoryReadResult {
    int id = 0;
    bool success = false;
    int beginRow = 0;
    QList<QSqlRecord> rows;
    HistoryArchive::DisplayedCursor cursor;
};
Q_DECLARE_METATYPE(HistoryReadResult)

class HistorySqlReadWorker final : public QObject {
    Q_OBJECT
public:
    explicit HistorySqlReadWorker(QString path) : path_(std::move(path)) { }

public slots:
    void open();
    void read(HistoryReadRequest request);
    void close();

signals:
    void completed(HistoryReadResult result);
    void stopped();

private:
    QString path_, connectionName_;
    QSqlDatabase db_;
};
