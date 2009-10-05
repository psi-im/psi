/*
 * pgpkeydlg.h 
 * Copyright (C) 2001-2009  Justin Karneges, Michail Pishchagin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
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

#include "pgpkeydlg.h"

#include <QString>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QKeyEvent>
#include <QHeaderView>

#include <pgputil.h>
#include "common.h"
#include "showtextdlg.h"

class KeyViewItem : public QStandardItem
{
public:
	KeyViewItem(const QCA::KeyStoreEntry& entry, const QString& name)
		: QStandardItem()
		, entry_(entry)
	{
		setText(name);
	}

	QCA::KeyStoreEntry entry() const
	{
		return entry_;
	}

private:
	QCA::KeyStoreEntry entry_;
};

class KeyViewProxyModel : public QSortFilterProxyModel
{
public:
	KeyViewProxyModel(QObject* parent)
		: QSortFilterProxyModel(parent)
	{
		setFilterCaseSensitivity(Qt::CaseInsensitive);
	}

	virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
	{
		for (int column = 0; column <= 1; ++column) {
			QModelIndex index = sourceModel()->index(sourceRow, column, sourceParent);
			if (index.data(Qt::DisplayRole).toString().contains(filterRegExp()))
				return true;
		}
		return false;
	}
};

PGPKeyDlg::PGPKeyDlg(Type t, const QString& defaultKeyID, QWidget *parent)
	: QDialog(parent)
	, model_(0)
{
	ui_.setupUi(this);
	setModal(true);

	pb_dtext_ = ui_.buttonBox->addButton(tr("&Diagnostics"), QDialogButtonBox::ActionRole);

	model_ = new QStandardItemModel(this);
	model_->setHorizontalHeaderLabels(QStringList()
	                                  << tr("Key ID")
	                                  << tr("User ID")
	                                 );
	proxy_ = new KeyViewProxyModel(this);
	proxy_->setSourceModel(model_);
	ui_.lv_keys->setModel(proxy_);

	ui_.lv_keys->header()->setResizeMode(QHeaderView::ResizeToContents);

	connect(ui_.lv_keys, SIGNAL(doubleClicked(const QModelIndex&)), SLOT(doubleClicked(const QModelIndex&)));
	connect(ui_.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(do_accept()));
	connect(ui_.buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), SLOT(reject()));
	connect(pb_dtext_, SIGNAL(clicked()), SLOT(show_ksm_dtext()));
	connect(ui_.le_filter, SIGNAL(textChanged(const QString&)), this, SLOT(filterTextChanged()));

	ui_.le_filter->installEventFilter(this);

	KeyViewItem* firstItem = 0;
	KeyViewItem* selectedItem = 0;
	int row = 0;

	foreach(QCA::KeyStore *ks, PGPUtil::instance().keystores_) {
		if (ks->type() == QCA::KeyStore::PGPKeyring && ks->holdsIdentities()) {
			foreach(QCA::KeyStoreEntry ke, ks->entryList()) {
				bool publicKey = (t == Public && ke.type() == QCA::KeyStoreEntry::TypePGPPublicKey) ||
				    (ke.type() == QCA::KeyStoreEntry::TypePGPSecretKey);
				bool secretKey = t == Secret && ke.type() == QCA::KeyStoreEntry::TypePGPSecretKey;
				if (publicKey || secretKey) {
					KeyViewItem *i = new KeyViewItem(ke,  ke.id().right(8));
					KeyViewItem *i2 = new KeyViewItem(ke, ke.name());
					QStandardItem* root = model_->invisibleRootItem();
					root->setChild(row, 0, i);
					root->setChild(row, 1, i2);
					++row;

					QString keyId;
					if (publicKey)
						keyId = ke.pgpPublicKey().keyId();
					else
						keyId = ke.pgpSecretKey().keyId();

					if (!defaultKeyID.isEmpty() && keyId == defaultKeyID) {
						selectedItem = i;
					}

					if (!firstItem) {
						firstItem = i;
					}
				}
			}
		}
	}

	if (selectedItem) {
		firstItem = selectedItem;
	}

	if (firstItem) {
		QModelIndex realIndex = model_->indexFromItem(firstItem);
		QModelIndex fakeIndex = proxy_->mapFromSource(realIndex);
		ui_.lv_keys->setCurrentIndex(fakeIndex);
		ui_.lv_keys->scrollTo(fakeIndex);
	}

	// adjustSize();
}

const QCA::KeyStoreEntry& PGPKeyDlg::keyStoreEntry() const
{
	return entry_;
}

bool PGPKeyDlg::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == ui_.le_filter && event->type() == QEvent::KeyPress) {
		QKeyEvent* ke = static_cast<QKeyEvent*>(event);
		if (ke->key() == Qt::Key_Up ||
		    ke->key() == Qt::Key_Down ||
		    ke->key() == Qt::Key_PageUp ||
		    ke->key() == Qt::Key_PageDown ||
		    ke->key() == Qt::Key_Home ||
		    ke->key() == Qt::Key_End)
		{
			QCoreApplication::instance()->sendEvent(ui_.lv_keys, event);
			return true;
		}
	}
	return QDialog::eventFilter(watched, event);
}

void PGPKeyDlg::filterTextChanged()
{
	proxy_->setFilterWildcard(ui_.le_filter->text());
}

void PGPKeyDlg::doubleClicked(const QModelIndex& index)
{
	ui_.lv_keys->setCurrentIndex(index);
	do_accept();
}

void PGPKeyDlg::do_accept()
{
	QModelIndex fakeIndex = ui_.lv_keys->currentIndex();
	QModelIndex realIndex = proxy_->mapToSource(fakeIndex);

	QStandardItem* item = model_->itemFromIndex(realIndex);
	KeyViewItem *i = dynamic_cast<KeyViewItem*>(item);
	if(!i) {
		QMessageBox::information(this, tr("Error"), tr("Please select a key."));
		return;
	}
	entry_ = i->entry();
	accept();
}

void PGPKeyDlg::show_ksm_dtext()
{
	QString dtext = QCA::KeyStoreManager::diagnosticText();
	ShowTextDlg *w = new ShowTextDlg(dtext, true, false, this);
	w->setWindowTitle(CAP(tr("Key Storage Diagnostic Text")));
	w->resize(560, 240);
	w->show();
}
