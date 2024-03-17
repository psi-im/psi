/*
 * Copyright (C) 2009  Barracuda Networks, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef GROUPCHATBROWSEWINDOW_H
#define GROUPCHATBROWSEWINDOW_H

#include <iris/xmpp_jid.h>

#include <QString>
#include <QWidget>

class GroupChatBrowseWindow : public QWidget {
    Q_OBJECT

public:
    class RoomOptions {
    public:
        XMPP::Jid jid;
        QString   roomName;
        bool      visible;

        RoomOptions() : visible(false) { }
    };

    class RoomInfo {
    public:
        XMPP::Jid jid;
        bool      remove;

        // the following are only valid if 'remove' is false
        QString roomName;
        bool    autoJoin;
        bool    owner;
        int     participants;

        RoomInfo() : remove(false), autoJoin(false), owner(false), participants(-1) { }
    };

    GroupChatBrowseWindow(QWidget *parent = 0) : QWidget(parent) { }

    // FIXME: remove this
    virtual QObject *controller() const                 = 0;
    virtual void     setController(QObject *controller) = 0;

    virtual void setGroupChatIcon(const QPixmap &icon)  = 0;
    virtual void setServer(const XMPP::Jid &roomServer) = 0;
    virtual void setServerVisible(bool b)               = 0;
    virtual void setNicknameVisible(bool b)             = 0;

signals:
    void onBrowse(const XMPP::Jid &roomServer);
    void onJoin(const XMPP::Jid &room);
    void onCreate(const XMPP::Jid &room);
    void onCreateConfirm(const GroupChatBrowseWindow::RoomOptions &options);
    void onCreateCancel(const XMPP::Jid &room);              // no ack
    void onCreateFinalize(const XMPP::Jid &room, bool join); // no-ack if join=false
    void onDestroy(const XMPP::Jid &room);
    void onSetAutoJoin(const QList<XMPP::Jid> &rooms, bool enabled);

public slots:
    // onBrowse
    virtual void handleBrowseResultsReady(const QList<GroupChatBrowseWindow::RoomInfo> &list) = 0;
    virtual void handleBrowseError(const QString &reason)                                     = 0;

    // onJoin or onCreateFinalize
    virtual void handleJoinSuccess()                    = 0;
    virtual void handleJoinError(const QString &reason) = 0;

    // from onCreate
    virtual void handleCreateSuccess(const GroupChatBrowseWindow::RoomOptions &defaultOptions) = 0;

    // from onCreateConfirm
    virtual void handleCreateConfirmed() = 0;

    // from onCreate or onCreateConfirm
    virtual void handleCreateError(const QString &reason) = 0;

    // from onDestroy
    virtual void handleDestroySuccess()                    = 0;
    virtual void handleDestroyError(const QString &reason) = 0;
};

class PsiGroupChatBrowseWindow : public GroupChatBrowseWindow {
    Q_OBJECT

public:
    PsiGroupChatBrowseWindow(QWidget *parent = 0);
    ~PsiGroupChatBrowseWindow();

    // FIXME: remove this
    virtual QObject *controller() const;
    virtual void     setController(QObject *controller);

    // from qwidget
    virtual void resizeEvent(QResizeEvent *event);

    virtual void setGroupChatIcon(const QPixmap &icon);
    virtual void setServer(const XMPP::Jid &roomServer);
    virtual void setServerVisible(bool b);
    virtual void setNicknameVisible(bool b);

public slots:
    virtual void handleBrowseResultsReady(const QList<GroupChatBrowseWindow::RoomInfo> &list);
    virtual void handleBrowseError(const QString &reason);
    virtual void handleJoinSuccess();
    virtual void handleJoinError(const QString &reason);
    virtual void handleCreateSuccess(const GroupChatBrowseWindow::RoomOptions &defaultOptions);
    virtual void handleCreateConfirmed();
    virtual void handleCreateError(const QString &reason);
    virtual void handleDestroySuccess();
    virtual void handleDestroyError(const QString &reason);

private:
    class Private;
    Private *d;
};

#endif // GROUPCHATBROWSEWINDOW_H
