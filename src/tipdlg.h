/*
 * tipdlg.h - handles tip of the day 
 * Copyright (C) 2001-2006  Michail Pishchagin
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


#ifndef TIPDLG_H
#define TIPDLG_H

#include <QDialog>
#include <QStringList>

#include "ui_tip.h"

class QString;
class PsiCon;

class TipDlg : public QDialog, public Ui::Tip
{
	Q_OBJECT
public:
	static void show(PsiCon* psi);

private:
	TipDlg(PsiCon* psi);
	~TipDlg();
	
public slots:
	void showTipsChanged(bool);
	void next();
	void previous();

protected:
	void updateTip();
	void addTip(const QString& tip, const QString& author);

private:
	PsiCon* psi_;
	QStringList tips;
};

#endif
