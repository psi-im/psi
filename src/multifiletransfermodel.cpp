/*
 * multifiletransfermodel.cpp - model for file transfers
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "multifiletransfermodel.h"

#include <QElapsedTimer>
#include <QDateTime>
#include <QIcon>

struct MultiFileTransferItem::Private
{
    QString       displayName;       // usually base filename
    QString       mediaType;
    QString       description;
    QString       errorString;       // last error
    quint64       fullSize;
    quint64       currentSize;       // currently transfered
    quint32       speed = 0;         // bytes per second
    quint32       timeRemaining = 0; // secs
    MultiFileTransferModel::Direction direction;
    MultiFileTransferModel::Status status = MultiFileTransferModel::Status::Pending;
    QIcon         thumbnail;
    QDateTime     startTime;
    QDateTime     finishTime;
    QElapsedTimer timer;             // last update
};

MultiFileTransferItem::MultiFileTransferItem(MultiFileTransferModel::Direction direction,
                                             const QString &displayName, quint64 fullSize) :
    d(new Private)
{
    d->direction   = direction;
    d->displayName = displayName;
    d->fullSize    = fullSize;
    d->startTime   = QDateTime::currentDateTime();
    d->timer.start();
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
    return d->speed;
}

MultiFileTransferModel::Direction MultiFileTransferItem::direction() const
{
    return d->direction;
}

MultiFileTransferModel::Status MultiFileTransferItem::status() const
{
    return d->status;
}

quint32 MultiFileTransferItem::timeRemaining() const
{
    return d->timeRemaining;
}

QString MultiFileTransferItem::errorString() const
{
    return d->errorString;
}

void MultiFileTransferItem::setCurrentSize(quint64 newCurrentSize)
{
    d->currentSize = newCurrentSize;
    // TODO compute progress etc
    emit updated(); // TODO only if necessary
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
    d->status = MultiFileTransferModel::Status::Failed;
    emit updated();
}

void MultiFileTransferItem::setSuccess()
{
    d->status = MultiFileTransferModel::Status::Done;
    emit updated();
}

MultiFileTransferModel::MultiFileTransferModel(QObject *parent) :
    QAbstractListModel(parent)
{
    connect(&updateTimer, &QTimer::timeout, this, [this](){
        auto s = updatedTransfers;
        updatedTransfers.clear();
        for (const auto &v: s) {
            int row = transfers.indexOf(v); // what about thousands of active transfers? probably we can build a map from trasfers
            if (row >= 0) {
                auto ind = index(row, 0, QModelIndex());
                emit dataChanged(ind, ind);
            }
        }
    });
}

MultiFileTransferModel::~MultiFileTransferModel()
{

}

int MultiFileTransferModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid()? 0 : transfers.size(); // only for root it's valid
}

QVariant MultiFileTransferModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    MultiFileTransferItem *item = static_cast<MultiFileTransferItem *>(index.internalPointer());
    if (role == Qt::DisplayRole) {
        return item->displayName();
    }
    if (role == Qt::DecorationRole) {
        return item->icon();
    }
    if (role == Qt::ToolTipRole) {
        return QString("<b>%1</b><br><br>%1<br><br>").arg(item->displayName(), item->description());
    }
    // try our roles
    switch (role) {
    case FullSizeRole:
        return item->fullSize();
    case CurrentSizeRole:
        return item->currentSize();
    case SpeedRole:
        return item->speed();
    case DescriptionRole:
        return item->description();
    case DirectionRole:
        return item->direction();
    case StatusRole:
        return item->status();
    case TimeRemainingRole:
        return item->timeRemaining();
    case ErrorStringRole:
        return item->errorString();

    // requests
    case RejectFileRole:
    case DeleteFileRole:
    case OpenDirRole:
    case OpenFileRole:
    default:
        break;
    }

    return QVariant();
}

bool MultiFileTransferModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return false;
    }
    MultiFileTransferItem *item = static_cast<MultiFileTransferItem *>(index.internalPointer());
    if (role == DescriptionRole) {
        item->setDescription(value.toString());
    } else if (role == RejectFileRole) {
        emit item->rejectRequested();
    } else if (role == DeleteFileRole) {
        emit item->deleteFileRequested();
    } else if (role == OpenDirRole) {
        emit item->openDirRequested();
    } else if (role == OpenFileRole) {
        emit item->openFileRequested();
    } else {
        return false;
    }
    return true;
}

QModelIndex MultiFileTransferModel::index(int row, int column, const QModelIndex &parent) const
{
    // copied from parent but added internal pointer
    return hasIndex(row, column, parent) ? createIndex(row, column, transfers[row]) : QModelIndex();
}

MultiFileTransferItem* MultiFileTransferModel::addTransfer(Direction direction,
                                                           const QString &displayName, quint64 fullSize)
{
    beginInsertRows(QModelIndex(), transfers.size(), transfers.size());
    auto t = new MultiFileTransferItem(direction, displayName, fullSize);
    connect(t, &MultiFileTransferItem::updated, this, [this, t](){
        updatedTransfers.insert(t);
        if (!updateTimer.isActive()) {
            updateTimer.start();
        }
    });
    connect(t, &MultiFileTransferItem::aboutToBeDeleted, this, [this,t](){
        auto i = transfers.indexOf(t);
        if (i >= 0) {
            beginRemoveRows(QModelIndex(), i, i);
            transfers.removeAt(i);
            updatedTransfers.remove(t);
            endRemoveRows();
        }
    });
    transfers.append(t);
    endInsertRows();
    return t;
}
