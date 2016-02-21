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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psitoolbar.h"

#include <QMenu>
#include <QLabel>
#include <QAction>
#include <QContextMenuEvent>
#include <QList>
#include <QMainWindow>
#include <QToolButton>

#include "iconset.h"
#include "psiiconset.h"
#include "iconaction.h"
#include "psioptions.h"
#include "options/opt_toolbars.h"
#include "common.h"

Qt::ToolBarArea dockPositionToToolBarArea(Qt3Dock dock)
{
	switch (dock) {
	case Qt3Dock_Top:
		return Qt::TopToolBarArea;
	case Qt3Dock_Bottom:
		return Qt::BottomToolBarArea;
	case Qt3Dock_Right:
		return Qt::RightToolBarArea;
	case Qt3Dock_Left:
		return Qt::LeftToolBarArea;

	case Qt3Dock_Unmanaged:
	case Qt3Dock_TornOff:
	case Qt3Dock_Minimized:
		;
	}

	return Qt::NoToolBarArea;
}

PsiToolBar::PsiToolBar(const QString& base, QWidget* mainWindow, MetaActionList* actionList)
	: QToolBar(mainWindow)
	, actionList_(actionList)
	, base_(base)
{
	Q_ASSERT(mainWindow);
	Q_ASSERT(actionList_);

	int s = PsiIconset::instance()->system().iconSize();
	setIconSize(QSize(s, s));

	customizeAction_ = new QAction(tr("&Configure Toolbar..."), this);
	connect(customizeAction_, SIGNAL(triggered()), this, SIGNAL(customize()));
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

QString PsiToolBar::base() const
{
	return base_;
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
	QList<QString> skipList;
	skipList << "button_options" << "button_status" << "event_notifier" << "spacer";

	foreach(QString actionName, o->getOption(base_ + ".actions").toStringList()) {
		IconAction* action = actions.action(actionName);

		if (action) {
			if (action->isSeparator()) {
				addSeparator();
			}
			else if (!skipList.contains(actionName)) {
				QToolButton *button = new QToolButton;
				button->setDefaultAction(action);
				button->setPopupMode(QToolButton::InstantPopup);
				addWidget(button);
			}
			else {
				action->addTo(this);
			}
		}
		else {
			qWarning("PsiToolBar::initialize(): action %s not found!", qPrintable(actionName));
		}
	}

	if (!PsiOptions::instance()->getOption("options.ui.tabs.grouping").toString().contains('A')) {
		QMainWindow* mainWindow = dynamic_cast<QMainWindow*>(parentWidget());
		if (mainWindow) {
			mainWindow->addToolBar(dockPositionToToolBarArea((Qt3Dock)o->getOption(base_ + ".dock.position").toInt()), this);
			if (o->getOption(base_ + ".dock.nl").toBool())
				mainWindow->insertToolBarBreak(this);
		}
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

void PsiToolBar::structToOptions(PsiOptions* options, const ToolbarPrefs& tb)
{
	Q_ASSERT(!tb.id.isEmpty());
	QString base = options->mapPut("options.ui.contactlist.toolbars", tb.id);
	options->setOption(base + ".name", tb.name);
	options->setOption(base + ".visible", tb.on);
	options->setOption(base + ".locked", tb.locked);
	// options->setOption(base + ".stretchable", tb.stretchable);
	options->setOption(base + ".actions", tb.keys);
	options->setOption(base + ".dock.position", tb.dock);
	// options->setOption(base + ".dock.index", tb.index);
	options->setOption(base + ".dock.nl", tb.nl);
	// options->setOption(base + ".dock.extra-offset", tb.extraOffset);
}
