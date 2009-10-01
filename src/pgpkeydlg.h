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

#ifndef PGPKEYDLG_H
#define PGPKEYDLG_H

#include <QtCrypto>

#include "ui_pgpkey.h"

class QStandardItemModel;
class QSortFilterProxyModel;

class PGPKeyDlg : public QDialog
{
	Q_OBJECT

public:
	enum Type { Public, Secret };

	PGPKeyDlg(Type, const QString& defaultKeyID, QWidget *parent = 0);
	const QCA::KeyStoreEntry& keyStoreEntry() const;

private slots:
	void doubleClicked(const QModelIndex& index);
	void filterTextChanged();
	void do_accept();
	void show_ksm_dtext();

protected:
	// reimplemented
	bool eventFilter(QObject* watched, QEvent* event);

private:
	Ui::PGPKey ui_;
	QCA::KeyStoreEntry entry_;
	QPushButton* pb_dtext_;
	QStandardItemModel* model_;
	QSortFilterProxyModel* proxy_;
};

#endif
