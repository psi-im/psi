/*
 * Copyright (C) 2008 Martin Hostettler
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Query dialog for password on login.


#include <QDebug>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>

#include "accountloginpassword.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psicontactlist.h"
#include "jidutil.h"
#include "alertmanager.h"


AccountLoginPassword::AccountLoginPassword(PsiAccount *account) : 
			account_(account),
			le_(this) {

	psi_ = account->psi();

	QString message;
	if (psi_->contactList()->enabledAccounts().count() > 1) {
		message = tr("Please enter the password for %1:").arg(JIDUtil::toString(account->jid(), true));
	} else {
		message = tr("Please enter your password:");
	}
	
	le_.setEchoMode(QLineEdit::Password);
	le_.setFocus();

	setWindowTitle(tr("Need Password"));
	QVBoxLayout *vbox = new QVBoxLayout(this);
	
	
	
	QLabel *label = new QLabel(message, this);
	vbox->addWidget(label);
	vbox->addStretch(1);
	
	vbox->addWidget(&le_);
	vbox->addStretch(1);
	
	label->setBuddy(&le_);
	
	
	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel,
														Qt::Horizontal, this);
	QPushButton *okButton = static_cast<QPushButton *>(buttonBox->addButton(QDialogButtonBox::Ok));
	okButton->setDefault(true);
	vbox->addWidget(buttonBox);
	
	QObject::connect(buttonBox, SIGNAL(accepted()), SLOT(accept()));
	QObject::connect(buttonBox, SIGNAL(rejected()), SLOT(reject()));
	
	setSizeGripEnabled(true);
	
	psi_->alertManager()->raiseDialog(this, AlertManager::AccountPassword);
}

void AccountLoginPassword::accept() {
	if (account_.isNull()) return; // not relevant anymore!
	QString password = le_.text();
	hide();
	if (!password.isEmpty()) {
		account_->passwordReady(password);
	} else {
		reject();
	}
	deleteLater();
}

void AccountLoginPassword::reject() {
	// if the user clicks 'online' in the
	//   status menu, then the online
	//   option will be 'checked' in the
	//   menu.  if the user cancels the
	//   password dialog, we call
	//   updateMainwinStatus to restore
	//   the status menu to the correct
	//   state.
	psi_->updateMainwinStatus();
	hide();
	deleteLater();
}
