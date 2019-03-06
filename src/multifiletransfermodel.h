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

#ifndef MULTIFILETRANSFERMODEL_H
#define MULTIFILETRANSFERMODEL_H

#include <QAbstractListModel>
#include <QScopedPointer>
#include <QSet>
#include <QTimer>

class MultiFileTransferItem;

class MultiFileTransferModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Direction {
        Incoming,
        Outgoing
    };

    enum Status {
        Pending,
        Active,
        Failed,
        Done
    };

    enum {
        FullSizeRole = Qt::UserRole,
        CurrentSizeRole,
        SpeedRole,
        DescriptionRole,
        DirectionRole,
        StatusRole,
        TimeRemainingRole,
        ErrorStringRole,

        // requests
        RejectFileRole,  // reject trasnfer of specific file
        DeleteFileRole,  // for finished-incoming files only (remove from disk)
        OpenDirRole,
        OpenFileRole
    };

    MultiFileTransferModel(QObject *parent);
    ~MultiFileTransferModel();

    int rowCount ( const QModelIndex & parent = QModelIndex() ) const ;
    QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QModelIndex index(int row, int column, const QModelIndex &parent) const;

    MultiFileTransferItem *addTransfer(Direction direction, const QString &displayName, quint64 fullSize);
private:
    QList<MultiFileTransferItem*> transfers;
    QSet<MultiFileTransferItem*> updatedTransfers;
    QTimer updateTimer;
};


class MultiFileTransferItem : public QObject
{
    Q_OBJECT
public:

    MultiFileTransferItem(MultiFileTransferModel::Direction direction, const QString &displayName, quint64 fullSize);
    ~MultiFileTransferItem();

    const QString &displayName() const;
    quint64 fullSize() const;
    quint64 currentSize() const;
    QIcon icon() const;
    QString mediaType() const;
    QString description() const;
    quint32 speed() const;
    MultiFileTransferModel::Direction direction() const;
    MultiFileTransferModel::Status status() const;
    quint32 timeRemaining() const;
    QString errorString() const;
    void setCurrentSize(quint64 newCurrentSize);
    void setThumbnail(const QIcon &img);
    void setMediaType(const QString &mediaType);
    void setDescription(const QString &description);
    void setFailure(const QString &error);
    void setSuccess();
signals:
    void descriptionChanged(); // user changes description
    void rejectRequested();    // user selects reject in UI
    void deleteFileRequested();// user selects delete file from context menu
    void openDirRequested();   // user wants to open directory with file
    void openFileRequested();  // user wants to open file

    void aboutToBeDeleted();   // just this object. mostly to notify the model
    void updated();            // to notify model mostly
private:
    friend class MultiFileTransferModel;
    struct Private;
    QScopedPointer<Private> d;
};


#endif // MULTIFILETRANSFERMODEL_H