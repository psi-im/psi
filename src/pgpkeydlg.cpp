/*
 * pgpkeydlg.h 
 * Copyright (C) 2001-2005  Justin Karneges
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

#include <Q3ListViewItem>
#include <QString>
#include <QMessageBox>
#include <QPushButton>

#include <pgputil.h>
#include "common.h"
#include "showtextdlg.h"

class KeyViewItem : public QTreeWidgetItem
{
public:
	KeyViewItem(const QCA::KeyStoreEntry& entry, QTreeWidget* parent)
		: QTreeWidgetItem(parent)
		, entry_(entry)
	{
		setText(0, entry_.id().right(8));
		setText(1, entry_.name());
	}

	QCA::KeyStoreEntry entry() const
	{
		return entry_;
	}

private:
	QCA::KeyStoreEntry entry_;
};


PGPKeyDlg::PGPKeyDlg(Type t, const QString& defaultKeyID, QWidget *parent) : QDialog(parent)
{
	ui_.setupUi(this);
	setModal(true);

	pb_dtext_ = ui_.buttonBox->addButton(tr("&Diagnostics"), QDialogButtonBox::ActionRole);

	ui_.lv_keys->header()->setResizeMode(QHeaderView::ResizeToContents);
	connect(ui_.lv_keys, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), SLOT(qlv_doubleClicked(QTreeWidgetItem *)));

	connect(ui_.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(do_accept()));
	connect(ui_.buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), SLOT(reject()));
	connect(pb_dtext_, SIGNAL(clicked()), SLOT(show_ksm_dtext()));

	KeyViewItem* firstItem = 0;
	KeyViewItem* selectedItem = 0;

	foreach(QCA::KeyStore *ks, PGPUtil::instance().keystores_) {
		if (ks->type() == QCA::KeyStore::PGPKeyring && ks->holdsIdentities()) {
			foreach(QCA::KeyStoreEntry ke, ks->entryList()) {
				bool publicKey = (t == Public && ke.type() == QCA::KeyStoreEntry::TypePGPPublicKey) ||
				    (ke.type() == QCA::KeyStoreEntry::TypePGPSecretKey);
				bool secretKey = t == Secret && ke.type() == QCA::KeyStoreEntry::TypePGPSecretKey;
				if (publicKey || secretKey) {
					KeyViewItem *i = new KeyViewItem(ke, ui_.lv_keys);

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
		ui_.lv_keys->setCurrentItem(firstItem);
		ui_.lv_keys->scrollToItem(firstItem);
	}

	// adjustSize();
}

const QCA::KeyStoreEntry& PGPKeyDlg::keyStoreEntry() const
{
	return entry_;
}

void PGPKeyDlg::qlv_doubleClicked(QTreeWidgetItem* i)
{
	ui_.lv_keys->setCurrentItem(i);
	do_accept();
}

void PGPKeyDlg::do_accept()
{
	KeyViewItem *i = static_cast<KeyViewItem*>(ui_.lv_keys->currentItem());
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
