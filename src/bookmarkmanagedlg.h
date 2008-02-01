/*
 * bookmarkmanagedlg.h - manage groupchat room bookmarks
 * Copyright (C) 2008  Michail Pishchagin
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

#ifndef BOOKMARKMANAGEDLG_H
#define BOOKMARKMANAGEDLG_H

#include <QDialog>

#include "ui_bookmarkmanage.h"

class PsiAccount;
class QPushButton;
class QStandardItemModel;
class QStandardItem;
class ConferenceBookmark;

#include "xmpp_jid.h"

class BookmarkManageDlg : public QDialog
{
	Q_OBJECT
public:
	BookmarkManageDlg(PsiAccount* account);
	~BookmarkManageDlg();

public slots:
	// reimplemented
	void reject();
	void accept();

private:
	enum Role {
		// DisplayRole / EditRole
		JidRole      = Qt::UserRole + 0,
		AutoJoinRole = Qt::UserRole + 1,
		NickRole     = Qt::UserRole + 2,
		PasswordRole = Qt::UserRole + 3
	};

	void loadBookmarks();
	void saveBookmarks();

	void appendItem(QStandardItem* item);
	XMPP::Jid jid() const;
	QModelIndex currentIndex() const;

private slots:
	void addBookmark();
	void removeBookmark();
	void updateCurrentItem();
	void joinCurrentRoom();
	void closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint);
	void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

private:
	Ui::BookmarkManage ui_;
	PsiAccount* account_;
	QStandardItemModel* model_;
	QPushButton* addButton_;
	QPushButton* removeButton_;
	QPushButton* joinButton_;

	ConferenceBookmark bookmarkFor(const QModelIndex& index) const;
};

#endif
