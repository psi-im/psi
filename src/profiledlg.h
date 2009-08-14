/*
 * profiledlg.h - dialogs for manipulating profiles
 * Copyright (C) 2001-2003  Justin Karneges
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

#ifndef PROFILEDLG_H
#define PROFILEDLG_H

#include <QDialog>

#include "ui_profileopen.h"
#include "ui_profilemanage.h"
#include "ui_profilenew.h"
#include "varlist.h"

class QButtonGroup;

class ProfileOpenDlg : public QDialog, public Ui::ProfileOpen
{
	Q_OBJECT
public:
	ProfileOpenDlg(const QString &, const VarList &, const QString &, QWidget *parent=0);
	~ProfileOpenDlg();

	QString newLang;

private slots:
	void manageProfiles();
	void langChange(int);

private:
	void reload(const QString &);

	VarList langs;
	int langSel;
};

class ProfileManageDlg : public QDialog, public Ui::ProfileManage
{
	Q_OBJECT
public:
	ProfileManageDlg(const QString &, QWidget *parent=0);

private slots:
	void slotProfileNew();
	void slotProfileRename();
	void slotProfileDelete();
	void updateSelection();
};

class ProfileNewDlg : public QDialog, public Ui::ProfileNew
{
	Q_OBJECT
public:
	ProfileNewDlg(QWidget *parent=0);

	QString name;

private slots:
	void slotCreate();
	void nameModified();

private:
	QButtonGroup* buttonGroup_;
};

#endif
