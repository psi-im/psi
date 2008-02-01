/*
 * bookmarkmanagedlg.cpp - manage groupchat room bookmarks
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

#include "bookmarkmanagedlg.h"

#include <QStandardItemModel>
#include <QPushButton>
#include <QAction>

#include "bookmarkmanager.h"
#include "psiaccount.h"

BookmarkManageDlg::BookmarkManageDlg(PsiAccount* account)
	: QDialog()
	, account_(account)
	, model_(0)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	ui_.setupUi(this);
	account_->dialogRegister(this);

	QAction* removeBookmarkAction = new QAction(this);
	connect(removeBookmarkAction, SIGNAL(activated()), SLOT(removeBookmark()));
	removeBookmarkAction->setShortcuts(QKeySequence::Delete);
	ui_.listView->addAction(removeBookmarkAction);

	addButton_    = ui_.bookmarkButtonBox->addButton(tr("&Add"),    QDialogButtonBox::ActionRole);
	removeButton_ = ui_.bookmarkButtonBox->addButton(tr("&Remove"), QDialogButtonBox::DestructiveRole);
	joinButton_   = ui_.bookmarkButtonBox->addButton(tr("&Join"),   QDialogButtonBox::ActionRole);
	connect(addButton_, SIGNAL(clicked()), SLOT(addBookmark()));
	connect(removeButton_, SIGNAL(clicked()), SLOT(removeBookmark()));
	connect(joinButton_, SIGNAL(clicked()), SLOT(joinCurrentRoom()));

	model_ = new QStandardItemModel(this);
	ui_.listView->setModel(model_);
	connect(ui_.listView->itemDelegate(), SIGNAL(closeEditor(QWidget*, QAbstractItemDelegate::EndEditHint)), SLOT(closeEditor(QWidget*, QAbstractItemDelegate::EndEditHint)));
	connect(ui_.listView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), SLOT(selectionChanged(const QItemSelection&, const QItemSelection&)));

	connect(ui_.host, SIGNAL(textEdited(const QString&)), SLOT(updateCurrentItem()));
	connect(ui_.room, SIGNAL(textEdited(const QString&)), SLOT(updateCurrentItem()));
	connect(ui_.nickname, SIGNAL(textEdited(const QString&)), SLOT(updateCurrentItem()));
	connect(ui_.password, SIGNAL(textEdited(const QString&)), SLOT(updateCurrentItem()));
	connect(ui_.autoJoin, SIGNAL(clicked(bool)), SLOT(updateCurrentItem()));

	loadBookmarks();

	QItemSelection dummy;
	selectionChanged(dummy, dummy);
}

BookmarkManageDlg::~BookmarkManageDlg()
{
	account_->dialogUnregister(this);
}

void BookmarkManageDlg::reject()
{
	close();
}

void BookmarkManageDlg::accept()
{
	if (account_->checkConnected(this)) {
		saveBookmarks();
		close();
	}
}

void BookmarkManageDlg::loadBookmarks()
{
	model_->clear();

	foreach(ConferenceBookmark c, account_->bookmarkManager()->conferences()) {
		QStandardItem* item = new QStandardItem(c.name());
		item->setData(QVariant(c.jid().full()), JidRole);
		item->setData(QVariant(c.autoJoin()),   AutoJoinRole);
		item->setData(QVariant(c.nick()),       NickRole);
		item->setData(QVariant(c.password()),   PasswordRole);
		appendItem(item);
	}
}

ConferenceBookmark BookmarkManageDlg::bookmarkFor(const QModelIndex& index) const
{
	return ConferenceBookmark(index.data(Qt::DisplayRole).toString(),
	                          index.data(JidRole).toString(),
	                          index.data(AutoJoinRole).toBool(),
	                          index.data(NickRole).toString(),
	                          index.data(PasswordRole).toString());
}

void BookmarkManageDlg::saveBookmarks()
{
	QList<ConferenceBookmark> conferences;
	for (int row = 0; row < model_->rowCount(); ++row) {
		QModelIndex index = model_->index(row, 0, QModelIndex());
		conferences += bookmarkFor(index);
	}

	account_->bookmarkManager()->setBookmarks(conferences);
}

void BookmarkManageDlg::addBookmark()
{
	QStandardItem* item = new QStandardItem(tr("Unnamed"));
	appendItem(item);
	ui_.listView->reset(); // ensure that open editors won't get in our way
	ui_.listView->setCurrentIndex(item->index());
	ui_.listView->edit(item->index());
}

void BookmarkManageDlg::removeBookmark()
{
	model_->removeRow(currentIndex().row());
}

void BookmarkManageDlg::closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint)
{
	if (hint == QAbstractItemDelegate::SubmitModelCache) {
		QList<QLineEdit*> lineEdits;
		lineEdits << ui_.host << ui_.room << ui_.nickname;
		foreach(QLineEdit* lineEdit, lineEdits) {
			if (lineEdit->text().isEmpty()) {
				lineEdit->setFocus();
				break;
			}
		}
	}
}

void BookmarkManageDlg::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	Q_UNUSED(deselected);

	QModelIndex current;
	if (!selected.indexes().isEmpty())
		current = selected.indexes().first();

	XMPP::Jid jid = XMPP::Jid(current.data(JidRole).toString());
	ui_.host->setText(jid.domain());
	ui_.room->setText(jid.node());
	ui_.nickname->setText(current.data(NickRole).toString());
	ui_.password->setText(current.data(PasswordRole).toString());
	ui_.autoJoin->setChecked(current.data(AutoJoinRole).toBool());
	QList<QWidget*> editors;
	editors << ui_.host << ui_.room << ui_.nickname << ui_.password << ui_.autoJoin;
	foreach(QWidget* w, editors) {
		w->setEnabled(current.isValid());
	}

	removeButton_->setEnabled(current.isValid());
	updateCurrentItem();
}

XMPP::Jid BookmarkManageDlg::jid() const
{
	XMPP::Jid jid;
	jid.set(ui_.host->text(), ui_.room->text());
	return jid;
}

void BookmarkManageDlg::updateCurrentItem()
{
	joinButton_->setEnabled(!jid().domain().isEmpty() && !jid().node().isEmpty() && !ui_.nickname->text().isEmpty());

	QStandardItem* item = model_->item(currentIndex().row());
	if (item) {
		item->setData(QVariant(jid().full()),              JidRole);
		item->setData(QVariant(ui_.autoJoin->isChecked()), AutoJoinRole);
		item->setData(QVariant(ui_.nickname->text()),      NickRole);
		item->setData(QVariant(ui_.password->text()),      PasswordRole);
	}
}

QModelIndex BookmarkManageDlg::currentIndex() const
{
	if (!ui_.listView->selectionModel()->selectedIndexes().isEmpty())
		return ui_.listView->selectionModel()->selectedIndexes().first();
	return QModelIndex();
}

void BookmarkManageDlg::joinCurrentRoom()
{
	account_->actionJoin(bookmarkFor(currentIndex()), true);
}

void BookmarkManageDlg::appendItem(QStandardItem* item)
{
	item->setDragEnabled(true);
	item->setDropEnabled(false);
	model_->invisibleRootItem()->appendRow(item);
}
