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

#include <Q3ListViewItem>
#include <QString>
#include <QMessageBox>

#include "pgpkeydlg.h"

class KeyViewItem : public Q3ListViewItem
{
public:
	KeyViewItem(const QCA::KeyStoreEntry& entry, Q3ListView *par) : Q3ListViewItem(par)
	{
		entry_ = entry;
	}

	QCA::KeyStoreEntry entry_;
};


PGPKeyDlg::PGPKeyDlg(Type t, const QString& defaultKeyID, QWidget *parent) : QDialog(parent)
{
	ui_.setupUi(this);
	setModal(true);

	connect(ui_.lv_keys, SIGNAL(doubleClicked(Q3ListViewItem *)), SLOT(qlv_doubleClicked(Q3ListViewItem *)));
	connect(ui_.pb_ok, SIGNAL(clicked()), SLOT(do_accept()));
	connect(ui_.pb_cancel, SIGNAL(clicked()), SLOT(reject()));

	Q3ListViewItem *isel = 0;
	foreach(QString k, QCA::keyStoreManager()->keyStores()) {
		QCA::KeyStore ks(k);
		if (ks.type() == QCA::KeyStore::PGPKeyring && ks.holdsIdentities()) {
			foreach(QCA::KeyStoreEntry ke, ks.entryList()) {
				if (t == Public && ke.type() == QCA::KeyStoreEntry::TypePGPPublicKey || ke.type() == QCA::KeyStoreEntry::TypePGPSecretKey) {
					KeyViewItem *i = new KeyViewItem(ke, ui_.lv_keys);
					i->setText(0, ke.id().right(8));
					i->setText(1, ke.name());
					if(!defaultKeyID.isEmpty() && ke.pgpPublicKey().keyId() == defaultKeyID) {
						ui_.lv_keys->setSelected(i, true);
						isel = i;
					}
				}
				else if (t == Secret && ke.type() == QCA::KeyStoreEntry::TypePGPSecretKey) {
					KeyViewItem *i = new KeyViewItem(ke, ui_.lv_keys);
					i->setText(0, ke.id().right(8));
					i->setText(1, ke.name());
					if(!defaultKeyID.isEmpty() && ke.pgpSecretKey().keyId() == defaultKeyID) {
						ui_.lv_keys->setSelected(i, true);
						isel = i;
					}
				}
			}
		}
	}

	if(ui_.lv_keys->childCount() > 0 && !isel)
		ui_.lv_keys->setSelected(ui_.lv_keys->firstChild(), true);
	else if(isel)
		ui_.lv_keys->ensureItemVisible(isel);
}

const QCA::KeyStoreEntry& PGPKeyDlg::keyStoreEntry() const
{
	return entry_;
}

void PGPKeyDlg::qlv_doubleClicked(Q3ListViewItem *i)
{
	ui_.lv_keys->setSelected(i, true);
	do_accept();
}

void PGPKeyDlg::do_accept()
{
	KeyViewItem *i = (KeyViewItem *)ui_.lv_keys->selectedItem();
	if(!i) {
		QMessageBox::information(this, tr("Error"), tr("Please select a key."));
		return;
	}
	entry_ = i->entry_;
	accept();
}
