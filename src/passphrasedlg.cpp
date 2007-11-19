/*
 * passphrasedlg.cpp - class to handle entering of OpenPGP passphrase
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

#include <QLineEdit>
#include <QPushButton>

#include "passphrasedlg.h"

PassphraseDlg::PassphraseDlg(QWidget *parent) : QDialog (parent)
{
	ui_.setupUi(this);
	setModal(false);
	connect(ui_.pb_ok, SIGNAL(clicked()), SLOT(accept()));
	connect(ui_.pb_cancel, SIGNAL(clicked()), SLOT(reject()));
}

void PassphraseDlg::promptPassphrase(const QString& name)
{
	setWindowTitle(tr("%1: OpenPGP Passphrase").arg(name));
	resize(minimumSizeHint());
}

QString PassphraseDlg::getPassphrase() const
{
	return ui_.le_pass->text();
}

