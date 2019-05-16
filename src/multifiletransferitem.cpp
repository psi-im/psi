/*
 * multifiletransferitem.cpp - file transfers item
 * Copyright (C) 2019 Sergey Ilinykh
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

#include <QElapsedTimer>
#include <QDateTime>
#include <QIcon>
#include <QContiguousCache>

struct MultiFileTransferItem::Private
{
    QString       displayName;       // usually base filename
    QString       mediaType;
    QString       description;
    QString       errorString;       // last error
    QString       fileName;
    quint64       fullSize = 0;
    quint64       currentSize = 0;   // currently transfered
    quint32       timeRemaining = 0; // secs
    MultiFileTransferModel::Direction direction;
    MultiFileTransferModel::State    state = MultiFileTransferModel::State::Pending;
    QIcon         thumbnail;
    QContiguousCache<quint32>         lastSpeeds = QContiguousCache<quint32>(3);      // bytes per second
    QElapsedTimer lastTimer;         // last speed value update
};

MultiFileTransferItem::MultiFileTransferItem(MultiFileTransferModel::Direction direction,
                                             const QString &displayName, quint64 fullSize) :
    d(new Private)
{
    d->direction   = direction;
    d->displayName = displayName;
    d->fullSize    = fullSize;
}

MultiFileTransferItem::~MultiFileTransferItem()
{
    emit aboutToBeDeleted();
}

const QString &MultiFileTransferItem::displayName() const
{
    return d->displayName;
}

quint64 MultiFileTransferItem::fullSize() const
{
    return d->fullSize;
}

quint64 MultiFileTransferItem::currentSize() const
{
    return d->currentSize;
}

QIcon MultiFileTransferItem::icon() const
{
    return d->thumbnail;
}

QString MultiFileTransferItem::mediaType() const
{
    return d->mediaType;
}

QString MultiFileTransferItem::description() const
{
    return d->description;
}

quint32 MultiFileTransferItem::speed() const
{
    if (d->lastSpeeds.size()) {
        quint64 sum = 0;
        for (int i = d->lastSpeeds.firstIndex(); i < d->lastSpeeds.lastIndex(); i++) {
            sum += d->lastSpeeds.at(i);
        }
        return quint32(sum / d->lastSpeeds.size());
    }
    return 0;
}

MultiFileTransferModel::Direction MultiFileTransferItem::direction() const
{
    return d->direction;
}

MultiFileTransferModel::State MultiFileTransferItem::state() const
{
    return d->state;
}

quint32 MultiFileTransferItem::timeRemaining() const
{
    return d->timeRemaining;
}

QString MultiFileTransferItem::errorString() const
{
    return d->errorString;
}

QString MultiFileTransferItem::toolTipText() const
{
    return QString("<b>%1</b><br><br>%2<br><br>").arg(d->displayName, d->description);
}

QString MultiFileTransferItem::filePath() const
{
    return d->fileName;
}

void MultiFileTransferItem::setCurrentSize(quint64 newCurrentSize)
{
    auto diff = newCurrentSize - d->currentSize;
    auto elapsed = d->lastTimer.elapsed();
    quint32 speed;
    if (diff && !elapsed) {
        speed = quint32(d->fullSize);
    } else if (diff) {
        speed = quint32(double(diff) / double(elapsed) / 1000.0);
    } else {
        speed = 0;
    }

    d->currentSize = newCurrentSize;
    d->lastSpeeds.append(speed);
    auto averageSpeed = this->speed();
    if (averageSpeed) {
        d->timeRemaining = (d->fullSize - d->currentSize) / averageSpeed;
    } else {
        d->timeRemaining = 0; // just undefined
    }
    d->lastTimer.start();
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

void MultiFileTransferItem::setFailure(const QString &error)
{
    d->errorString = error;
    d->state = MultiFileTransferModel::State::Failed;
    emit updated();
}

void MultiFileTransferItem::setSuccess()
{
    d->state = MultiFileTransferModel::State::Done;
    emit updated();
}

void MultiFileTransferItem::setState(MultiFileTransferModel::State state, const QString &stateComment)
{
    d->state = state;
    d->errorString = stateComment;
    if (state == MultiFileTransferModel::Active) {
        d->lastTimer.start();
    }
}

void MultiFileTransferItem::setFileName(const QString &fileName)
{
    d->fileName = fileName;
}
