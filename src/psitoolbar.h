/*
 * psitoolbar.h - the Psi toolbar class
 * Copyright (C) 2003-2008  Michail Pishchagin
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

#ifndef PSITOOLBAR_H
#define PSITOOLBAR_H

#include <QToolBar>

class QContextMenuEvent;

class ToolbarPrefs;
class PsiOptions;

#include "psiactionlist.h"

class PsiToolBar : public QToolBar
{
	Q_OBJECT

public:
	PsiToolBar(const QString& base, QWidget* mainWindow, MetaActionList* actionList);
	~PsiToolBar();

	void initialize();
	void updateVisibility();
	QString base() const;

	static void structToOptions(PsiOptions* options, const ToolbarPrefs& s);

signals:
	void customize();

protected:
	void contextMenuEvent(QContextMenuEvent* e);

private:
	MetaActionList* actionList_;
	QAction* customizeAction_;
	QString base_;
};

#endif
