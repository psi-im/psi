/*
 * bookmarksmanagerdlg.h - dialog for managing Bookmarks
 * stored in private storage
 * Copyright (C) 2006, 2008 Cestonaro Thilo, Kevin Smith
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

#include <QDialog>
#include <QList>

#include "psiaccount.h"
#include "urlbookmark.h"
#include "conferencebookmark.h"
#include "bookmarkmanager.h"

#ifndef BOOKMARKSMANAGERDLG_H
#define BOOKMARKSMANAGERDLG_H

#include "ui_bookmarksmanagerdlg.h"

#define ARRAYINDEX Qt::UserRole 

class BookmarksManagerDlg : public QDialog, public Ui::BookmarksManagerDlg
{
	Q_OBJECT
public:
	BookmarksManagerDlg(PsiAccount *pa);
	~BookmarksManagerDlg();
	class Private;

private:
	Private *d;
	friend class Private;
    bool validateForm();

protected:
	void getToplevelItems(QTreeWidgetItem **topUrl, QTreeWidgetItem **topConference);
	void addItem(QTreeWidgetItem *parent, int data, QString theName, QString theUrlOrJid);

public slots:
	void onAdd();
	void onModify();
	void onRemove();
	void onAccept();
	void onReject();
	void onRBClicked();
	void onItemSelectionChanged();
	void onItemDoubleClicked(QTreeWidgetItem *, int);
	void getBookmarks_success(const QList<URLBookmark>&, const QList<ConferenceBookmark>&);
	void getBookmarks_error(int, const QString&);
	void setBookmarks_success();
	void setBookmarks_error(int, const QString&);
};
#endif
