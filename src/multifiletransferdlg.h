/*
 * multifiletransferdlg.h - file transfer dialog
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

#ifndef MULTIFILETRANSFERDLG_H
#define MULTIFILETRANSFERDLG_H

#include <QDialog>
#include <QScopedPointer>

class MultiFileTransferItem;
class PsiAccount;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;

namespace Ui {
    class MultiFileTransferDlg;
}

namespace XMPP {
    class Jid;
    namespace Jingle {
        class Session;
        namespace FileTransfer {
            class Application;
        }
    }
}

class BinaryUriLoader : public QObject
{
    Q_OBJECT
public:
    BinaryUriLoader(PsiAccount *acc, const XMPP::Jid &peer, const QUrl &uri);
signals:
    void ready(const QByteArray &);
};

class MultiFileTransferDlg : public QDialog
{
    Q_OBJECT

public:
    explicit MultiFileTransferDlg(PsiAccount *acc, QWidget *parent = nullptr);
    ~MultiFileTransferDlg();

    void initOutgoing(const XMPP::Jid &jid, const QStringList &fileList);
    void initIncoming(XMPP::Jingle::Session *session);

    void reject();
    void accept();

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dropEvent(QDropEvent *event);

private:
    void updatePeerVisuals();
    void updateMyVisuals();
    void updateComonVisuals();
    void addTransferContent(MultiFileTransferItem *item);
    void appendOutgoing(const QStringList &fileList);
    void setupSessionSignals();
    void setupCommonSignals(XMPP::Jingle::FileTransfer::Application *app, MultiFileTransferItem *item);

private:
    Ui::MultiFileTransferDlg *ui;
    class Private;
    QScopedPointer<Private> d;
};

#endif // MULTIFILETRANSFERDLG_H
