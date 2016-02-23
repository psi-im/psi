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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "bookmarkmanagedlg.h"

#include <QStandardItemModel>
#include <QPushButton>
#include <QAction>
#include <QFile>
#include <QMessageBox>

#include "bookmarkmanager.h"
#include "psiaccount.h"
#include "fileutil.h"
#include "iconset.h"

BookmarkManageDlg::BookmarkManageDlg(PsiAccount* account)
	: QDialog()
	, account_(account)
	, model_(0)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
	ui_.setupUi(this);
	account_->dialogRegister(this);

	QAction* removeBookmarkAction = new QAction(this);
	connect(removeBookmarkAction, SIGNAL(triggered()), SLOT(removeBookmark()));
	removeBookmarkAction->setShortcuts(QKeySequence::Delete);
	ui_.listView->addAction(removeBookmarkAction);

	ui_.autoJoin->addItems(ConferenceBookmark::joinTypeNames());

	ui_.pb_import->setIcon(IconsetFactory::icon("psi/import").icon());
	ui_.pb_export->setIcon(IconsetFactory::icon("psi/export").icon());
	connect(ui_.pb_import, SIGNAL(clicked()), SLOT(importBookmarks()));
	connect(ui_.pb_export, SIGNAL(clicked()), SLOT(exportBookmarks()));

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
	connect(ui_.autoJoin, SIGNAL(currentIndexChanged(int)), SLOT(updateCurrentItem()));

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
	QDialog::reject();
}

void BookmarkManageDlg::accept()
{
	QStandardItem* item = model_->item(ui_.listView->currentIndex().row());
	if(item && item->data(Qt::DisplayRole).toString().isEmpty())
		item->setData(QVariant(item->data(JidRole)), Qt::DisplayRole);

	if (account_->checkConnected(this)) {
		saveBookmarks();
		QDialog::accept();
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
							  (ConferenceBookmark::JoinType)index.data(AutoJoinRole).toInt(),
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
	Q_UNUSED(editor);

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
	QModelIndex current;
	if (!selected.indexes().isEmpty())
		current = selected.indexes().first();

	if(!deselected.isEmpty()) {
		QStandardItem* item = model_->item(deselected.indexes().first().row());
		if(item && item->data(Qt::DisplayRole).toString().isEmpty())
			item->setData(QVariant(item->data(JidRole)), Qt::DisplayRole);
	}

	XMPP::Jid jid = XMPP::Jid(current.data(JidRole).toString());
	ui_.host->setText(jid.domain());
	ui_.room->setText(jid.node());
	ui_.nickname->setText(current.data(NickRole).toString());
	ui_.password->setText(current.data(PasswordRole).toString());
	ui_.autoJoin->setCurrentIndex(current.data(AutoJoinRole).toInt());
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
	return XMPP::Jid(ui_.room->text(), ui_.host->text());
}

void BookmarkManageDlg::updateCurrentItem()
{
	joinButton_->setEnabled(!jid().domain().isEmpty() && !jid().node().isEmpty() && !ui_.nickname->text().isEmpty());

	QStandardItem* item = model_->item(currentIndex().row());
	if (item) {
		item->setData(QVariant(jid().full()),              JidRole);
		item->setData(QVariant(ui_.autoJoin->currentIndex()), AutoJoinRole);
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

void BookmarkManageDlg::importBookmarks()
{
	QString fileName = FileUtil::getOpenFileName(this, tr("Import bookmarks"));
	if(fileName.isEmpty())
		return;

	QFile file(fileName);
	if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QDomDocument doc;
		QString error;
		if(doc.setContent(&file, &error)) {
			QDomElement root = doc.firstChildElement("bookmarks");
			if(root.isNull())
				return;
			QDomElement elem = root.firstChildElement("conference");
			while(!elem.isNull()) {
				ConferenceBookmark c(elem);

				QStandardItem* item = new QStandardItem(c.name());
				item->setData(QVariant(c.jid().full()), JidRole);
				item->setData(QVariant(c.autoJoin()),   AutoJoinRole);
				item->setData(QVariant(c.nick()),       NickRole);
				item->setData(QVariant(c.password()),   PasswordRole);
				appendItem(item);

				elem = elem.nextSiblingElement("conference");
			}
		}
		else {
			QMessageBox::warning(this, tr("Error!"), error);
		}
	}
}

void BookmarkManageDlg::exportBookmarks()
{
	QString fileName = FileUtil::getSaveFileName(this, tr("Export bookmarks"), "bookmarks.txt");
	if(fileName.isEmpty())
		return;

	QFile file(fileName);
	if(file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		QDomDocument doc;
		QDomElement root = doc.createElement("bookmarks");
		doc.appendChild(root);
		for (int row = 0; row < model_->rowCount(); ++row) {
			QModelIndex index = model_->index(row, 0, QModelIndex());
			ConferenceBookmark cb = bookmarkFor(index);
			root.appendChild(cb.toXml(doc));
		}
		file.write(doc.toString().toLocal8Bit());
	}
}
