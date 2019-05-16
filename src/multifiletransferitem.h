/*
 * multifiletransferitem.h - file transfer item
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

#ifndef MULTIFILETRANSFERITEM_H
#define MULTIFILETRANSFERITEM_H

#include "multifiletransfermodel.h"

#include <QScopedPointer>

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
    MultiFileTransferModel::State state() const;
    quint32 timeRemaining() const;
    QString errorString() const;
    QString toolTipText() const;
    QString filePath() const;
    void setThumbnail(const QIcon &img);
    void setMediaType(const QString &mediaType);
    void setDescription(const QString &description);
    void setFailure(const QString &error);
    void setSuccess();
    void setState(MultiFileTransferModel::State state, const QString &stateComment = QString());
    void setFileName(const QString &filePath);
public slots:
    void setCurrentSize(quint64 newCurrentSize);
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


#endif // MULTIFILETRANSFERITEM_H
