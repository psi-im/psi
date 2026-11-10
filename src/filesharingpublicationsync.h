/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "filesharingcatalog.h"
#include "historymediaqueries.h"

#include <QObject>
#include <QTimer>
#include <functional>

// History is authoritative. Its worker returns bounded value snapshots; this
// GUI-side coordinator reconstructs announcements and commits one catalog batch.
// Cursors advance only after success. Reconciliation preserves explicit serving.
class FileSharingPublicationSync : public QObject {
    Q_OBJECT
public:
    struct HistoryAccess {
        std::function<void(qint64, int, QObject *, std::function<void(QList<HistoryMediaRefs::Link>, bool)>)> readLinks;
        std::function<void(const QList<QByteArray> &, QObject *, std::function<void(QSet<QByteArray>, bool)>)>
            readLinked;
    };
    FileSharingPublicationSync(HistoryAccess history, FileSharingCatalog *catalog, QObject *parent = nullptr);
    bool isIdle() const { return !_busy && !_timer.isActive(); }

public slots:
    void linksAdded();
    void reconcile();

private:
    void step();
    void sweep(quint64 generation);
    void finish(bool success);
    bool current(quint64 generation);

    HistoryAccess                     _history;
    FileSharingCatalog               *_catalog;
    QTimer                            _timer;
    qint64                            _linkPosition = 0;
    FileSharingCatalog::PublicationId _sweepPosition;
    quint64                           _generation = 0;
    bool                              _sweeping = false, _scanLinks = false, _busy = false, _reportedError = false;
};
