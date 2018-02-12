/*
 * gcuserview.h - groupchat roster
 * Copyright (C) 2001, 2002  Justin Karneges
 * 2011 Evgeny Khryukin
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

#ifndef GCUSERVIEW_H
#define GCUSERVIEW_H

#include <QAbstractItemModel>
#include <QTreeView>

#include "xmpp_status.h"

using namespace XMPP;

class GCUserView;
class PsiAccount;
namespace XMPP {
class Jid;
}

class GCUserModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Role { Moderator = 0, Participant = 1, Visitor = 2, LastGroupRole };

    enum {
        StatusRole = Qt::UserRole,
        AvatarRole,
        ClientIconRole,
        AffilationIconRole,
    };

    class MUCContact {
    public:
        typedef QSharedPointer<MUCContact> Ptr;
        QString name;
        Status  status;
        QPixmap avatar;
    };

    GCUserModel(PsiAccount *account, const Jid selfJid, QObject *parent);



    // added
    void removeEntry(const QString &nick);
    void updateEntry(const QString &nick, const Status &);
    GCUserModel::MUCContact *findEntry(const QString &) const;

    void clear();
    bool hasJid(const Jid&);
    QStringList nickList() const;
    MUCContact *selfContact() const;
    void updateAvatar(const QString &nick);

    // reimplemented
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QMimeData* mimeData(const QModelIndexList &indexes) const;
    Qt::DropActions supportedDragActions() const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;

public slots:
    void updateAll();

signals:
    void contextMenuRequested(const QString &nick);

private:
    QModelIndex findIndex(const QString &nick) const;
    QString makeToolTip(const MUCContact &contact) const;
    static Role groupRole(const Status &s);

private:
    QList<MUCContact::Ptr> contacts[LastGroupRole]; // splitted into groups

    PsiAccount *_account;
    Jid _selfJid;
    QString _selfNick;
    MUCContact::Ptr _selfContact;
};

class GCUserView : public QTreeView
{
    Q_OBJECT
public:
    GCUserView(QWidget* parent);
    ~GCUserView();

    void setLooks();

protected:
    void mousePressEvent(QMouseEvent *event);
    void contextMenuEvent(QContextMenuEvent *cm);

signals:
    void action(const QString &nick, const Status &, int actionType);
    void insertNick(const QString& nick);
    void contextMenuRequested(const QString &nick);

private slots:
    void qlv_doubleClicked(const QModelIndex& index);
};

#endif
