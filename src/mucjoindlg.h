/*
 * mucjoindlg.h
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

#ifndef MUCJOINDLG_H
#define MUCJOINDLG_H

#include <QDialog>

#include "ui_mucjoin.h"

class PsiCon;
class QString;
class PsiAccount;

class MUCJoinDlg : public QDialog, public Ui::MUCJoin
{
	Q_OBJECT

public:
	MUCJoinDlg(PsiCon *, PsiAccount *);
	~MUCJoinDlg();

	void joined();
	void error(int, const QString &);

protected:
	//void closeEvent(QCloseEvent *);

public slots:
	void done(int);
	void doJoin();

private slots:
	void updateIdentity(PsiAccount *);
	void updateIdentityVisibility();
	void pa_disconnected();
	void recent_activated(int);

private:
	class Private;
	Private *d;

	void disableWidgets();
	void enableWidgets();
};

#endif
