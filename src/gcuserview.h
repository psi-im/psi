/*
 * gcuserview.h - groupchat roster
 * Copyright (C) 2001, 2002  Justin Karneges
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef GCUSERVIEW_H
#define GCUSERVIEW_H

#include <Q3ListView>

#include "xmpp_status.h"

using namespace XMPP;

class QPainter;
class GCMainDlg;
class GCUserView;
class GCUserViewGroupItem;
namespace XMPP {
	class Jid;
}

class GCUserViewItem : public QObject, public Q3ListViewItem
{
public:
	GCUserViewItem(GCUserViewGroupItem *);
	void paintFocus(QPainter *, const QColorGroup &, const QRect &);
	void paintBranches(QPainter *p, const QColorGroup &cg, int w, int, int h);

	Status s;
};

class GCUserViewGroupItem : public Q3ListViewItem
{
public:
	GCUserViewGroupItem(GCUserView *, const QString&, int);
	void paintFocus(QPainter *, const QColorGroup &, const QRect &);
	void paintBranches(QPainter *p, const QColorGroup &cg, int w, int, int h);
	void paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int alignment);
	int compare(Q3ListViewItem *i, int col, bool ascending ) const;
private:
	int key_;
};

class GCUserView : public Q3ListView
{
	Q_OBJECT
public:
	GCUserView(QWidget* parent);
	~GCUserView();

	void setMainDlg(GCMainDlg* mainDlg);
	Q3DragObject* dragObject();
	void clear();
	void updateAll();
	bool hasJid(const Jid&);
	Q3ListViewItem *findEntry(const QString &);
	void updateEntry(const QString &, const Status &);
	void removeEntry(const QString &);
	QStringList nickList() const;

protected:
	enum Role { Moderator = 0, Participant = 1, Visitor = 2 };

	GCUserViewGroupItem* findGroup(XMPP::MUCItem::Role a) const;
	bool maybeTip(const QPoint &);
	bool event(QEvent* e);

signals:
	void action(const QString &nick, const Status &, int actionType);
	void insertNick(const QString& nick);

private slots:
	void qlv_doubleClicked(Q3ListViewItem *);
	void qlv_contextMenuRequested(Q3ListViewItem *, const QPoint &, int);
	void qlv_mouseButtonClicked(int button, Q3ListViewItem* item, const QPoint& pos, int c);

private:
	GCMainDlg* gcDlg_;
};

#endif
