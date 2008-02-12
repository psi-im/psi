/*
 * psitoolbar.cpp - the Psi toolbar class
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "psitoolbar.h"

#include <QMenu>
#include <QLabel>
#include <QAction>
#include <QContextMenuEvent>
#include <QList>
#include <QMainWindow>

#include "iconset.h"
#include "iconaction.h"
#include "psioptions.h"
#include "options/opt_toolbars.h"
#include "common.h"

Qt::ToolBarArea dockPositionToToolBarArea(Qt::Dock dock)
{
	switch (dock) {
	case Qt::DockTop:
		return Qt::TopToolBarArea;
	case Qt::DockBottom:
		return Qt::BottomToolBarArea;
	case Qt::DockRight:
		return Qt::RightToolBarArea;
	case Qt::DockLeft:
		return Qt::LeftToolBarArea;

	case Qt::DockUnmanaged:
	case Qt::DockTornOff:
	case Qt::DockMinimized:
		;
	}

	return Qt::NoToolBarArea;
}

PsiToolBar::PsiToolBar(const QString& base, QMainWindow* mainWindow, MetaActionList* actionList)
	: QToolBar(mainWindow)
	, actionList_(actionList)
	, base_(base)
{
	Q_ASSERT(mainWindow);
	Q_ASSERT(actionList_);

	setIconSize(QSize(16, 16));

	customizeAction_ = new QAction(tr("Configure& Toolbar..."), this);
	connect(customizeAction_, SIGNAL(activated()), this, SIGNAL(customize()));
}

PsiToolBar::~PsiToolBar()
{
}

void PsiToolBar::contextMenuEvent(QContextMenuEvent* e)
{
	e->accept();

	QMenu menu;
	menu.addAction(customizeAction_);
	menu.exec(e->globalPos());
}

void PsiToolBar::initialize()
{
	PsiOptions* o = PsiOptions::instance();
	if (o->getOption(base_ + ".key").toString().isEmpty()) {
		o->setOption(base_ + ".key", ToolbarPrefs().id);
	}
	setObjectName(QString("mainwin-toolbar-%1").arg(o->getOption(base_ + ".key").toString()));
	setMovable(!o->getOption(base_ + ".locked").toBool());
	setWindowTitle(o->getOption(base_ + ".name").toString());

	ActionList actions = actionList_->suitableActions(PsiActionList::Actions_MainWin | PsiActionList::Actions_Common);
	foreach(QString actionName, o->getOption(base_ + ".actions").toStringList()) {
		IconAction* action = actions.action(actionName);

		if (action) {
			if (action->isSeparator()) {
				addSeparator();
			}
			else {
				action->addTo(this);
			}
		}
		else {
			qWarning("PsiToolBar::initialize(): action %s not found!", qPrintable(actionName));
		}
	}

	QMainWindow* mainWindow = dynamic_cast<QMainWindow*>(parentWidget());
	mainWindow->addToolBar(dockPositionToToolBarArea((Qt::Dock)o->getOption(base_ + ".dock.position").toInt()),
	                       this);
	if (mainWindow && o->getOption(base_ + ".dock.nl").toBool()) {
		mainWindow->insertToolBarBreak(this);
	}

	updateVisibility();
}

void PsiToolBar::updateVisibility()
{
	if (PsiOptions::instance()->getOption(base_ + ".visible").toBool()) {
		show();
	}
	else {
		hide();
	}
}

void PsiToolBar::structToOptions(const ToolbarPrefs& tb)
{
	Q_ASSERT(!tb.id.isEmpty());
	PsiOptions* o = PsiOptions::instance();
	QString base = o->mapPut("options.ui.contactlist.toolbars", tb.id);
	o->setOption(base + ".name", tb.name);
	o->setOption(base + ".visible", tb.on);
	o->setOption(base + ".locked", tb.locked);
	// o->setOption(base + ".stretchable", tb.stretchable);
	o->setOption(base + ".actions", tb.keys);
	o->setOption(base + ".dock.position", tb.dock);
	// o->setOption(base + ".dock.index", tb.index);
	o->setOption(base + ".dock.nl", tb.nl);
	// o->setOption(base + ".dock.extra-offset", tb.extraOffset);
}
