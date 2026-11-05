/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingpublicationsync.h"

#include "historymediarefs.h"

#include <QSqlDatabase>

namespace {
const int BatchSize = 100;
const int StepDelay = 10; // ms, so that a long pass leaves the GUI thread alone
}

FileSharingPublicationSync::FileSharingPublicationSync(const QString &historyConnection, FileSharingCatalog *catalog,
                                                       QObject *parent) :
    QObject(parent), _history(historyConnection), _catalog(catalog)
{
    _timer.setSingleShot(true);
    _timer.setInterval(StepDelay);
    connect(&_timer, &QTimer::timeout, this, &FileSharingPublicationSync::step);
}

void FileSharingPublicationSync::linksAdded() { _timer.start(); }

void FileSharingPublicationSync::reconcile()
{
    _linkPosition = 0;
    _sweepPosition.clear();
    _sweeping = true;
    _timer.start();
}

void FileSharingPublicationSync::step()
{
    const auto history = QSqlDatabase::database(_history, false);
    if (!history.isOpen())
        return;
    const bool moreLinks        = followLinks(history);
    const bool morePublications = sweepPublications(history);
    if (moreLinks || morePublications)
        _timer.start();
}

bool FileSharingPublicationSync::followLinks(QSqlDatabase history)
{
    const auto links = HistoryMediaRefs::linksAfter(history, _linkPosition, BatchSize);
    if (links.isEmpty())
        return false;
    QList<FileSharingCatalog::PublicationId> ids;
    for (const auto &link : links)
        ids.append(link.publicationId);
    for (const auto &id : _catalog->missing(ids)) {
        const auto told = HistoryMediaRefs::announcement(history, id);
        if (told && !_catalog->add(id, told->account, told->announcer, told->attachment))
            qWarning("Could not persist a shared file publication");
    }
    _linkPosition = links.last().position;
    return links.size() == BatchSize;
}

bool FileSharingPublicationSync::sweepPublications(QSqlDatabase history)
{
    if (!_sweeping)
        return false;
    const auto ids = _catalog->announcementsAfter(_sweepPosition, BatchSize);
    if (ids.size() < BatchSize)
        _sweeping = false;
    if (ids.isEmpty())
        return false;
    const auto linked = HistoryMediaRefs::linked(history, ids);
    if (!linked) {
        _sweeping = false;
        return false;
    }
    QList<FileSharingCatalog::PublicationId> forgotten;
    for (const auto &id : ids)
        if (!linked->contains(id))
            forgotten.append(id);
    if (!forgotten.isEmpty())
        _catalog->remove(forgotten);
    _sweepPosition = ids.last();
    return _sweeping;
}
