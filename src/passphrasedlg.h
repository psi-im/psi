/*
 * passphrasedlg.h - class to handle entering of OpenPGP passphrase
 * Copyright (C) 2003  Justin Karneges
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

#ifndef PASSPHRASEDLG_H
#define PASSPHRASEDLG_H

#include <QtCrypto>

#include "ui_passphrase.h"

class PassphraseDlg : public QDialog, public Ui::Passphrase
{
	Q_OBJECT
public:
	PassphraseDlg(const QString& name, const QString& entryId, QCA::EventHandler*, int requestId, QWidget *parent=0);

public slots:
	void reject();
	void accept();

private:
	QString entryId_;
	QCA::EventHandler* eventHandler_;
	int requestId_;
};

#endif
