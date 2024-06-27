/*
 * chatviewcommon.h - shared part of any chatview
 * Copyright (C) 2010  Sergey Ilinykh
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

#ifndef CHATVIEWBASE_H
#define CHATVIEWBASE_H

#include <QColor>
#include <QDateTime>
#include <QMap>
#include <QStringList>

class QWidget;

class ChatViewCommon {
public:
    enum UserType { LocalParty, RemoteParty, Participant };

    ChatViewCommon() : _nickNumber(0) { }
    void                    setLooks(QWidget *);
    inline const QDateTime &lastMsgTime() const { return _lastMsgTime; }
    bool                    updateLastMsgTime(QDateTime t);
    QString                 getMucNickColor(const QString &, bool);
    QList<QColor>           getPalette();

protected:
    // a cache of reactions per message
    struct Reactions {
        QMap<QString, QStringList>    total;   // unicode reaction => nicknames
        QHash<QString, QSet<QString>> perUser; // nickname => unicode reactions
    };

    struct ReactionsItem {
        QString     base;
        QString     code;
        QStringList nicks;
    };

    void addUser(const QString &nickname);
    void removeUser(const QString &nickname);
    void renameUser(const QString &oldNickname, const QString &newNickname);

    // takes incoming reactions and returns reactions to be send to UI
    QList<ReactionsItem> updateReactions(const QString &senderNickname, const QString &messageId,
                                         const QSet<QString> &reactions);

    // to be called from UI stuff. return list of reactions to send over network
    QSet<QString> onReactionSwitched(const QString &senderNickname, const QString &messageId, const QString &reaction);

protected:
    QDateTime _lastMsgTime;

private:
    QList<QColor>            &generatePalette();
    bool                      compatibleColors(const QColor &, const QColor &);
    int                       _nickNumber;
    QMap<QString, int>        _nicks;
    QHash<QString, Reactions> _reactions; // messageId -> reactions
};

#endif // CHATVIEWBASE_H
