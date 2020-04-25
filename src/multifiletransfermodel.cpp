/*
 * multifiletransfermodel.cpp - model for file transfers
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

#include "multifiletransfermodel.h"

#include "multifiletransferitem.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QIcon>

// ----------------------------------------------------------------
// MultiFileTransferModel
// ----------------------------------------------------------------
MultiFileTransferModel::MultiFileTransferModel(QObject *parent) : QAbstractListModel(parent)
{
    updateTimer.setSingleShot(true);
    updateTimer.setInterval(1000);
    connect(&updateTimer, &QTimer::timeout, this, [this]() {
        auto s = updatedTransfers;
        updatedTransfers.clear();
        for (const auto &v : s) {
            v->updateStats();
            int row = transfers.indexOf(
                v); // what about thousands of active transfers? probably we can build a map from trasfers
            if (row >= 0) {
                auto ind = index(row, 0, QModelIndex());
                emit dataChanged(ind, ind);
            }
        }
    });
}

MultiFileTransferModel::~MultiFileTransferModel() { }

Qt::ItemFlags MultiFileTransferModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
    }
    return QAbstractListModel::flags(index);
}

int MultiFileTransferModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : transfers.size() + (addEnabled ? 1 : 0); // only for root it's valid
}

QVariant MultiFileTransferModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() == transfers.size()) { // only possible when we have "+" button as the last item
        if (role == StateRole) {
            return AddTemplate;
        }
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
        return item->toolTipText();
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
    case StateRole:
        return item->state();
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
    return hasIndex(row, column, parent) ? createIndex(row, column, row == transfers.size() ? nullptr : transfers[row])
                                         : QModelIndex();
}

QHash<int, QByteArray> MultiFileTransferModel::roleNames() const
{
    return QHash<int, QByteArray> {
        { Qt::DisplayRole, "display" },         { Qt::DecorationRole, "decoration" }, { Qt::ToolTipRole, "toolTip" },
        { FullSizeRole, "fullSize" },           { CurrentSizeRole, "currentSize" },   { SpeedRole, "speed" },
        { DescriptionRole, "description" },     { DirectionRole, "direction" },       { StateRole, "stateRole" },
        { TimeRemainingRole, "timeRemaining" }, { ErrorStringRole, "errorString" },
    };
}

void MultiFileTransferModel::clear()
{
    beginResetModel();
    qDeleteAll(transfers);
    endResetModel();
}

MultiFileTransferItem *MultiFileTransferModel::addTransfer(Direction direction, const QString &displayName,
                                                           quint64 fullSize)
{
    beginInsertRows(QModelIndex(), transfers.size(), transfers.size());
    auto t = new MultiFileTransferItem(direction, displayName, fullSize, this);
    connect(t, &MultiFileTransferItem::updated, this, [this, t]() {
        updatedTransfers.insert(t);
        if (!updateTimer.isActive()) {
            updateTimer.start();
        }
    });
    connect(t, &MultiFileTransferItem::aboutToBeDeleted, this, [this, t]() {
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

void MultiFileTransferModel::forEachTransfer(const std::function<void(MultiFileTransferItem *)> cb) const
{
    for (auto &t : transfers) {
        cb(t);
    }
}

void MultiFileTransferModel::setAddEnabled(bool enabled)
{
    if (addEnabled == enabled) {
        return;
    }
    if (enabled) {
        beginInsertRows(QModelIndex(), transfers.size() + 1, transfers.size() + 1);
    } else {
        beginRemoveRows(QModelIndex(), transfers.size() + 1, transfers.size() + 1);
    }
    addEnabled = enabled;
    if (enabled) { // wasn't enabled. now enabled. +1 row
        endInsertRows();
    } else {
        endRemoveRows();
    }
}

bool MultiFileTransferModel::isAddEnabled() const { return addEnabled; }
