/*
 * filesharedlg.h - file sharing dialog
 * Copyright (C) 2001-2019  Psi Team
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

#ifndef FILESHAREDLG_H
#define FILESHAREDLG_H

#include <QDialog>

#include "xmpp_reference.h"

namespace XMPP {
    class Message;
}

namespace Ui {
class FileShareDlg;
}
class QMimeData;
class PsiAccount;
class MultiFileTransferModel;
class FileSharingItem;

class FileShareDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FileShareDlg(const QList<FileSharingItem*> &items, QWidget *parent = nullptr);
    ~FileShareDlg();

    QString description() const;
    inline bool hasPublishErrors() const { return hasFailures; }

    static FileShareDlg* fromMimeData(const QMimeData *md, PsiAccount *acc, QWidget *parent);

    void showImage(const QImage &img);
    QList<FileSharingItem *> takeItems();
public slots:
    void publish();

signals:
    void published(); // signalled when all items are published or failed

private:
    Ui::FileShareDlg *ui;
    PsiAccount *account;
    QImage image;
    MultiFileTransferModel *filesModel;
    QList<FileSharingItem*> readyPublishers;
    int inProgressCount = 0;
    bool hasFailures = false;
};

#endif // FILESHAREDLG_H
