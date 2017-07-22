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

#include <QTreeWidget>

#include "xmpp_status.h"

using namespace XMPP;

class GCMainDlg;
class GCUserView;
class GCUserViewGroupItem;
namespace XMPP {
	class Jid;
}

class GCUserViewItem : public QObject, public QTreeWidgetItem
{
public:
	GCUserViewItem(GCUserViewGroupItem *);
	void setAvatar(const QPixmap& pix);
	QPixmap avatar() const { return avatar_; };
	void setIcon(const QPixmap &icon);
	QPixmap icon() const { return icon_; };

	Status s;

	virtual bool operator<  (const QTreeWidgetItem& it) const;

private:
	QPixmap avatar_;
	QPixmap icon_;
};

class GCUserViewGroupItem : public QTreeWidgetItem
{
public:
	GCUserViewGroupItem(GCUserView *, const QString&, int);

	void updateText();
	int key() const { return key_; };

private:
	int key_;
	QString baseText_;
};

class GCUserView : public QTreeWidget
{
	Q_OBJECT
public:
	GCUserView(QWidget* parent);
	~GCUserView();

	void setMainDlg(GCMainDlg* mainDlg);
	GCMainDlg* mainDlg() const { return gcDlg_; };
	virtual QMimeData* mimeData(const QList<QTreeWidgetItem*>items) const;
	void clear();
	void updateAll();
	bool hasJid(const Jid&);
	QTreeWidgetItem *findEntry(const QString &);
	QTreeWidgetItem *findEntry(const QModelIndex& index);
	void updateEntry(const QString &, const Status &);
	void removeEntry(const QString &);
	QStringList nickList() const;
	void doContextMenu(QTreeWidgetItem* it);
	void setLooks();

protected:
	enum Role { Moderator = 0, Participant = 1, Visitor = 2 };

	GCUserViewGroupItem* findGroup(XMPP::MUCItem::Role a) const;
	bool maybeTip(const QPoint &);
	bool event(QEvent* e);
	void mousePressEvent(QMouseEvent *event);
	void contextMenuEvent(QContextMenuEvent *cm);

signals:
	void action(const QString &nick, const Status &, int actionType);
	void insertNick(const QString& nick);

private slots:
	void qlv_doubleClicked(const QModelIndex& index);

private:
	GCMainDlg* gcDlg_;
};

#endif
