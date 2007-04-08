/*
 * accountmodifydlg.h
 * Copyright (C) 2001, 2002  Justin Karneges
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

#ifndef ACCOUNTMODIFYDLG_H
#define ACCOUNTMODIFYDLG_H

#include <QDialog>
#include <QtCrypto>

#include "privacylistmodel.h"
#include "privacylistblockedmodel.h"
#include "ui_accountmodify.h"

class PsiAccount;
class QWidget;
class ProxyChooser;

class AccountModifyDlg : public QDialog, public Ui::AccountModify
{
	Q_OBJECT
public:
	AccountModifyDlg(PsiAccount *, QWidget *parent=0);
	~AccountModifyDlg();

	void setPassword(const QString &);

private slots:
	void hostToggled(bool);
	void sslToggled(bool);
	void legacySSLToggled(bool);

	void detailsVCard();
	void detailsChangePW();

	void save();

	//void pgpToggled(bool);
	void chooseKey();
	void clearKey();
 	void tabChanged(int);

	// Privacy
 	void privacyClicked();
 	void updatePrivacyTab();
 	void setPrivacyTabEnabled(bool b);
 	void addBlockClicked();
 	void removeBlockClicked();
 	void updateBlockedContacts(const PrivacyList&);
 	void getDefaultList_error();
 	void changeList_error();

private:
	PsiAccount *pa;
	ProxyChooser *pc;
	QCA::PGPKey key;
	
	// Privacy
 	PrivacyListModel privacyModel;
 	PrivacyListBlockedModel privacyBlockedModel;
 	bool privacyInitialized;

	void updateUserID();
	void setKeyID(bool b, const QString &s="");
	bool checkSSL();
};

#endif
