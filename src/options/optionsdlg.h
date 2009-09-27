/*
 * optionsdlg.cpp
 * Copyright (C) 2003-2009  Michail Pishchagin
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

#ifndef OPTIONSDLG_H
#define OPTIONSDLG_H

#include "ui_ui_options.h"
#include <QDialog>

class PsiCon;

class OptionsDlg : public QDialog, public Ui::OptionsUI
{
	Q_OBJECT
public:
	OptionsDlg(PsiCon *, QWidget *parent = 0);
	~OptionsDlg();

	void openTab(const QString& id);

signals:
	void applyOptions();

private slots:
	void doOk();
	void doApply();

public:
	class Private;
private:
	Private *d;
	friend class Private;

	QPushButton* pb_apply;
};

#endif
