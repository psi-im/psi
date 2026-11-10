/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingpublicationsync.h"

#include "historymediarefs.h"

#include <QDebug>
#include <QSqlError>
#include <utility>

namespace {
constexpr int BatchSize  = 100;
constexpr int StepDelay  = 10;
constexpr int RetryDelay = 1000;
}

FileSharingPublicationSync::FileSharingPublicationSync(HistoryAccess history, FileSharingCatalog *catalog,
                                                       QObject *parent) :
    QObject(parent), _history(std::move(history)), _catalog(catalog)
{
    _timer.setSingleShot(true);
    connect(&_timer, &QTimer::timeout, this, &FileSharingPublicationSync::step);
}

void FileSharingPublicationSync::linksAdded()
{
    _scanLinks = true;
    // Sustained message traffic must not keep restarting an active timer.
    if (!_busy && !_timer.isActive())
        _timer.start(StepDelay);
}

void FileSharingPublicationSync::reconcile()
{
    ++_generation;
    _linkPosition = 0;
    _sweepPosition.clear();
    _sweeping = true;
    linksAdded();
}

bool FileSharingPublicationSync::current(quint64 generation)
{
    if (generation == _generation)
        return true;
    // An erase/reconcile superseded this snapshot. Its successor owns the
    // cursors; never publish an obsolete response after clearing history.
    _busy = false;
    _timer.start(StepDelay);
    return false;
}

void FileSharingPublicationSync::step()
{
    if (_busy)
        return;
    _busy                 = true;
    const auto generation = _generation;
    if (!_scanLinks) {
        sweep(generation);
        return;
    }
    _scanLinks = false;
    _history.readLinks(
        _linkPosition, BatchSize, this, [this, generation](QList<HistoryMediaRefs::Link> links, bool ok) {
            if (!current(generation))
                return;
            if (!ok) {
                _scanLinks = true;
                finish(false);
                return;
            }
            QList<FileSharingCatalog::PublicationId> ids;
            for (const auto &link : links)
                ids.append(link.publicationId);
            QSqlError  error;
            const auto missing = _catalog->missing(ids, &error);
            if (error.isValid()) {
                _scanLinks = true;
                finish(false);
                return;
            }
            QSet<QByteArray> needed;
            for (const auto &id : missing)
                needed.insert(id);
            QList<FileSharingCatalog::Announcement> announcements;
            for (const auto &link : links) {
                if (!needed.contains(link.publicationId))
                    continue;
                const auto told = HistoryMediaRefs::announcement(link);
                if (!told) {
                    _scanLinks = true;
                    finish(false);
                    return;
                }
                announcements.append({ link.publicationId, told->account, told->announcer, told->file });
            }
            if (!_catalog->addBatch(announcements)) {
                _scanLinks = true;
                finish(false);
                return;
            }
            if (!links.isEmpty())
                _linkPosition = links.last().position;
            _scanLinks = _scanLinks || links.size() == BatchSize;
            sweep(generation);
        });
}

void FileSharingPublicationSync::sweep(quint64 generation)
{
    if (!_sweeping) {
        finish(true);
        return;
    }
    QSqlError  error;
    const auto ids = _catalog->announcementsAfter(_sweepPosition, BatchSize, &error);
    if (error.isValid()) {
        finish(false);
        return;
    }
    if (ids.isEmpty()) {
        _sweeping = false;
        finish(true);
        return;
    }
    _history.readLinked(ids, this, [this, generation, ids](QSet<QByteArray> linked, bool ok) {
        if (!current(generation))
            return;
        if (!ok) {
            finish(false);
            return;
        }
        QList<FileSharingCatalog::PublicationId> forgotten;
        for (const auto &id : ids)
            if (!linked.contains(id))
                forgotten.append(id);
        if (!_catalog->remove(forgotten)) {
            finish(false);
            return;
        }
        _sweepPosition = ids.last();
        _sweeping      = ids.size() == BatchSize;
        finish(true);
    });
}

void FileSharingPublicationSync::finish(bool success)
{
    _busy = false;
    if (!success) {
        if (!_reportedError)
            qWarning("Could not reconcile file publications; retrying without advancing the cursor");
        _reportedError = true;
        _timer.start(RetryDelay);
    } else {
        _reportedError = false;
        if (_scanLinks || _sweeping)
            _timer.start(StepDelay);
    }
}
