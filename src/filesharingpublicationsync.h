/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "filesharingcatalog.h"

#include <QObject>
#include <QTimer>

class QSqlDatabase;

// Keeps the publications of stored messages in line with history, which links each
// attachment to a publication and knows nothing else about it. A link without a
// publication gets one, told again from the stored message, and a publication nothing
// links to goes away. So new messages, messages stored by an older Psi, a lost file
// database and erased history are all one case, and a crash between the two databases
// needs no special handling. Works in small steps on the GUI thread, which owns both.
class FileSharingPublicationSync : public QObject {
    Q_OBJECT
public:
    FileSharingPublicationSync(const QString &historyConnection, FileSharingCatalog *catalog,
                               QObject *parent = nullptr);

    bool isIdle() const { return !_timer.isActive(); }

public slots:
    // History stored new links.
    void linksAdded();
    // Goes through everything: at start, and when history lost links.
    void reconcile();

private:
    void step();
    bool followLinks(QSqlDatabase history);
    bool sweepPublications(QSqlDatabase history);

    QString                           _history;
    FileSharingCatalog               *_catalog;
    QTimer                            _timer;
    qint64                            _linkPosition = 0;
    FileSharingCatalog::PublicationId _sweepPosition;
    bool                              _sweeping = false;
};
