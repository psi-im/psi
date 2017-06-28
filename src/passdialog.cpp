/*
 * passdialog.cpp
 * Copyright (C) 2009-2010 Virnik, 2011 Evgeny Khryukin
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

#include <QCheckBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "passdialog.h"


PassDialog::PassDialog(const QString& jid, QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Wrong Account Password"));

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setMargin(6);
	mainLayout->setSpacing(6);
	QHBoxLayout *botLayout = new QHBoxLayout();

	le_password = new QLineEdit();
	le_password->setEchoMode(QLineEdit::Password);

	cb_savePassword = new QCheckBox(tr("Save Password"));
	botLayout->addWidget(cb_savePassword);
	botLayout->addStretch(1);

	QPushButton *pb_ok = new QPushButton(tr("OK"));
	pb_ok->setDefault(true);
	botLayout->addWidget(pb_ok);

	QPushButton *pb_cancel = new QPushButton(tr("Cancel"));
	botLayout->addWidget(pb_cancel);

	mainLayout->addWidget(new QLabel(tr("Please enter your password for %1:").arg(jid)));
	mainLayout->addWidget(le_password);
	mainLayout->addLayout(botLayout);

	connect(pb_cancel, SIGNAL(clicked()), this, SLOT(reject()));
	connect(pb_ok, SIGNAL(clicked()), this, SLOT(accept()));
}

bool PassDialog::savePassword() const
{
	return cb_savePassword->isChecked();
}

void PassDialog::setSavePassword(bool save)
{
	cb_savePassword->setChecked(save);
}

QString PassDialog::password() const
{
	return le_password->text();
}
