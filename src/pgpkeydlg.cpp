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

#include <QListWidgetItem>
#include <QString>
#include <QMessageBox>

#include <pgputil.h>
#include "pgpkeydlg.h"
#include "common.h"
#include "showtextdlg.h"

class KeyViewItem : public QListWidgetItem
{
public:
	KeyViewItem(const QCA::KeyStoreEntry& entry, QListWidget *par) : QListWidgetItem(par)
	{
		entry_ = entry;
	}

	QCA::KeyStoreEntry entry_;
};


PGPKeyDlg::PGPKeyDlg(Type t, const QString& defaultKeyID, QWidget *parent) : QDialog(parent)
{
#if 0
	ui_.setupUi(this);
	setModal(true);

	connect(ui_.lv_keys, SIGNAL(doubleClicked(QListWidgetItem *)), SLOT(qlv_doubleClicked(QListWidgetItem *)));
	connect(ui_.pb_ok, SIGNAL(clicked()), SLOT(do_accept()));
	connect(ui_.pb_cancel, SIGNAL(clicked()), SLOT(reject()));
	connect(ui_.pb_dtext, SIGNAL(clicked()), SLOT(show_ksm_dtext()));

	QListWidgetItem *isel = 0;

	foreach(QCA::KeyStore *ks, PGPUtil::instance().keystores_) {
		if (ks->type() == QCA::KeyStore::PGPKeyring && ks->holdsIdentities()) {
			foreach(QCA::KeyStoreEntry ke, ks->entryList()) {
				if ((t == Public && ke.type() == QCA::KeyStoreEntry::TypePGPPublicKey) || (ke.type() == QCA::KeyStoreEntry::TypePGPSecretKey)) {
					KeyViewItem *i = new KeyViewItem(ke, ui_.lv_keys);
					// i->setText(0, ke.id().right(8));
					// i->setText(1, ke.name());
					i->setText(QString("%1 %2").arg(ke.id().right(8))
					           .arg(ke.name());
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
#endif
}

const QCA::KeyStoreEntry& PGPKeyDlg::keyStoreEntry() const
{
	return entry_;
}

void PGPKeyDlg::qlv_doubleClicked(QListWidgetItem *i)
{
#if 0
	ui_.lv_keys->setSelected(i, true);
	do_accept();
#endif
}

void PGPKeyDlg::do_accept()
{
#if 0
	KeyViewItem *i = (KeyViewItem *)ui_.lv_keys->selectedItem();
	if(!i) {
		QMessageBox::information(this, tr("Error"), tr("Please select a key."));
		return;
	}
	entry_ = i->entry_;
	accept();
#endif
}

void PGPKeyDlg::show_ksm_dtext()
{
	QString dtext = QCA::KeyStoreManager::diagnosticText();
	ShowTextDlg *w = new ShowTextDlg(dtext, true, false, this);
	w->setWindowTitle(CAP(tr("Key Storage Diagnostic Text")));
	w->resize(560, 240);
	w->show();
}
