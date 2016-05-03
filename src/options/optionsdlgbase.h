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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef OPTIONSDLGBASE_H
#define OPTIONSDLGBASE_H

#include "ui_ui_options.h"
#include <QDialog>

class OptionsTab;
class PsiCon;

class OptionsDlgBase : public QDialog, public Ui::OptionsUI
{
	Q_OBJECT
public:
	OptionsDlgBase(PsiCon *, QWidget *parent = 0);
	~OptionsDlgBase();

	PsiCon *psi() const;
	void openTab(const QString& id);

protected:
	void setTabs(QList<OptionsTab*> tabs); /* can be called from constructor */

signals:
	void applyOptions();

private slots:
	void doOk();
	void doApply();
	void enableCommonControls(bool enable = true);

public:
	class Private;
private:
	Private *d;
	friend class Private;

	QPushButton* pb_apply;
};

#endif
