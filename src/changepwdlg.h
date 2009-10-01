/*
 * changepwdlg.h - dialog for changing password
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

#ifndef CHANGEPWDLG_H
#define CHANGEPWDLG_H

#include "ui_changepw.h"

class PsiAccount;

class ChangePasswordDlg : public QDialog
{
	Q_OBJECT
public:
	ChangePasswordDlg(PsiAccount *, QWidget *parent=0);
	~ChangePasswordDlg();

protected:
	//void closeEvent(QCloseEvent *e);

public slots:
	void done(int);

private slots:
	void apply();
	void finished();
	void disc();

private:
	void blockWidgets();
	void restoreWidgets();
	void setWidgetsEnabled(bool enabled);

	Ui::ChangePassword ui_;
	PsiAccount* pa;
};

#endif
