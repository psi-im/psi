/*
 * multifiletransferitem.cpp - file transfers item
 * Copyright (C) 2019  Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "multifiletransferitem.h"

#include <QContiguousCache>
#include <QDateTime>
#include <QElapsedTimer>
#include <QIcon>

struct MultiFileTransferItem::Private {
    QString                           displayName; // usually base filename
    QString                           mediaType;
    QString                           description;
    QString                           info;
    QString                           errorString; // last error
    QString                           fileName;
    quint64                           fullSize      = 0;
    quint64                           currentSize   = 0; // currently transferred
    quint64                           lastSize      = 0;
    quint64                           offset        = 0; // initial offset if only part of file is transferred
    quint32                           timeRemaining = 0; // secs
    quint32                           speed;             // bytes per second
    MultiFileTransferModel::Direction direction;
    MultiFileTransferModel::State     state = MultiFileTransferModel::State::Pending;
    QIcon                             thumbnail;
    QContiguousCache<quint32>         lastSpeeds = QContiguousCache<quint32>(5); // bytes per second
    QElapsedTimer                     lastTimer;                                 // last speed value update
};

MultiFileTransferItem::MultiFileTransferItem(MultiFileTransferModel::Direction direction, const QString &displayName,
                                             quint64 fullSize, QObject *parent) :
    QObject(parent),
    d(new Private)
{
    d->direction   = direction;
    d->displayName = displayName;
    d->fullSize    = fullSize;
}

MultiFileTransferItem::~MultiFileTransferItem() { emit aboutToBeDeleted(); }

const QString &MultiFileTransferItem::displayName() const { return d->displayName; }

quint64 MultiFileTransferItem::fullSize() const { return d->fullSize; }

quint64 MultiFileTransferItem::currentSize() const { return d->currentSize; }

quint64 MultiFileTransferItem::offset() const { return d->offset; }

QIcon MultiFileTransferItem::icon() const { return d->thumbnail; }

QString MultiFileTransferItem::mediaType() const { return d->mediaType; }

QString MultiFileTransferItem::description() const { return d->description; }

quint32 MultiFileTransferItem::speed() const { return d->speed; }

MultiFileTransferModel::Direction MultiFileTransferItem::direction() const { return d->direction; }

MultiFileTransferModel::State MultiFileTransferItem::state() const { return d->state; }

quint32 MultiFileTransferItem::timeRemaining() const { return d->timeRemaining; }

QString MultiFileTransferItem::errorString() const { return d->errorString; }

QString MultiFileTransferItem::toolTipText() const
{
    QString text = QString("<b>%1</b>").arg(d->displayName);
    if (!d->description.isEmpty()) {
        text += (QLatin1String("<br><br>") + d->description);
    }
    text += (QLatin1String("<br><br>")
             + tr("Transferred: %1/%2 bytes").arg(QString::number(d->currentSize), QString::number(d->fullSize)));
    if (!d->info.isEmpty()) {
        text += (QLatin1String("<br><br>") + d->info);
    }
    return text;
}

QString MultiFileTransferItem::filePath() const { return d->fileName; }

QIcon MultiFileTransferItem::thumbnail() const { return d->thumbnail; }

void MultiFileTransferItem::setCurrentSize(quint64 newCurrentSize)
{
    d->currentSize = newCurrentSize;
    emit updated();
}

void MultiFileTransferItem::setThumbnail(const QIcon &img)
{
    d->thumbnail = img;
    emit updated();
}

void MultiFileTransferItem::setMediaType(const QString &mediaType)
{
    d->mediaType = mediaType;
    emit updated();
}

void MultiFileTransferItem::setDescription(const QString &description)
{
    d->description = description;
    emit descriptionChanged();
    emit updated();
}

void MultiFileTransferItem::setInfo(const QString &html) { d->info = html; }

void MultiFileTransferItem::setFailure(const QString &error)
{
    d->errorString = error;
    d->state       = MultiFileTransferModel::State::Failed;
    emit updated();
}

void MultiFileTransferItem::setSuccess()
{
    d->state = MultiFileTransferModel::State::Done;
    emit updated();
}

void MultiFileTransferItem::setState(MultiFileTransferModel::State state, const QString &stateComment)
{
    d->state       = state;
    d->errorString = stateComment;
    if (state == MultiFileTransferModel::Active) {
        d->lastTimer.start();
    }
}

void MultiFileTransferItem::setFileName(const QString &fileName) { d->fileName = fileName; }

void MultiFileTransferItem::setOffset(quint64 offset)
{
    d->offset      = offset;
    d->lastSize    = offset;
    d->currentSize = offset;
}

void MultiFileTransferItem::updateStats()
{
    auto elapsed = d->lastTimer.elapsed();
    if (!elapsed) {
        return;
    }
    double speedf = double(d->currentSize - d->lastSize) * 1000.0 / double(elapsed); // bytes per second
    d->lastSpeeds.append(uint(speedf));

    quint64 sum = 0;
    for (int i = d->lastSpeeds.firstIndex(); i < d->lastSpeeds.lastIndex(); i++) {
        sum += d->lastSpeeds.at(i);
    }
    d->speed = quint32(sum / qulonglong(d->lastSpeeds.size()));

    d->timeRemaining = quint32((d->fullSize - d->currentSize) / speedf);
    d->lastSize      = d->currentSize;
    d->lastTimer.start();
}
