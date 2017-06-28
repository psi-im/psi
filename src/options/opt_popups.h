/*
 * opt_popups.h
 * Copyright (C) 2011  Evgeny Khryukin
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

#ifndef OPT_POPUPS_H
#define OPT_POPUPS_H

#include "optionstab.h"

class PsiCon;
class PopupManager;

class OptionsTabPopups : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabPopups(QObject *parent);

	QWidget *widget();
	void applyOptions();
	void restoreOptions();

	virtual bool stretchable() const { return true; }

public slots:
	virtual void setData(PsiCon *psi, QWidget *);

private:
	QWidget *w;
	PopupManager* popup_;
};

#endif
