/*
 * psitoolbar.cpp - the Psi toolbar class
 * Copyright (C) 2003, 2004  Michail Pishchagin
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

#include "common.h"
#include "psitoolbar.h"
#include <q3popupmenu.h>
#include <qlabel.h>
#include <qaction.h>
//Added by qt3to4:
#include <QContextMenuEvent>
#include <Q3PtrList>
#include <Q3MainWindow>
#include "psicon.h"
#include "iconset.h"
#include "psicon.h"
#include "iconaction.h"
#include "psioptions.h"
#include "options/opt_toolbars.h"

//----------------------------------------------------------------------------
// PsiToolBar::Private
//----------------------------------------------------------------------------

class PsiToolBar::Private
{
public:
	Private();

	bool customizeable, moveable;
	PsiActionList::ActionsType type;
	PsiCon *psi;
	Q3PtrList<IconAction> uniqueActions;
	QString base;
	bool dirty;
};

PsiToolBar::Private::Private()
{
	customizeable = true;
	moveable = true;
	psi = 0;
	dirty = false;
	uniqueActions.setAutoDelete( true );
}

//----------------------------------------------------------------------------
// PsiToolBar
//----------------------------------------------------------------------------

PsiToolBar *PsiToolBar::fromOptions(const QString &base, Q3MainWindow* mainWindow, PsiCon* psi, PsiActionList::ActionsType t)
{
	PsiToolBar *tb = 0;

	tb = new PsiToolBar(PsiOptions::instance()->getOption(base + ".name").toString(),
	                    mainWindow, psi);
	mainWindow->moveDockWindow(tb,
	                           (Qt::Dock)PsiOptions::instance()->getOption(base + ".dock.position").toInt(),
	                           PsiOptions::instance()->getOption(base + ".dock.nl").toBool(),
	                           PsiOptions::instance()->getOption(base + ".dock.index").toInt(),
	                           PsiOptions::instance()->getOption(base + ".dock.extra-offset").toInt());

	tb->setType(t);
	tb->initialize(base, false);
	return tb;
}


PsiToolBar::PsiToolBar(const QString& label, Q3MainWindow* mainWindow, PsiCon* psi)
		: Q3ToolBar(label, mainWindow, (QWidget*)mainWindow)
{
	d = new Private();
	d->psi = psi;
	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
	connect(PsiOptions::instance(), SIGNAL(optionRemoved(const QString&)), SLOT(optionRemoved(const QString&)));
}

PsiToolBar::~PsiToolBar()
{
	delete d;
}


void PsiToolBar::optionChanged(const QString& option)
{
	if (d->dirty) return;
	if (!option.startsWith(d->base + ".")) return;

	d->dirty = true;
	// collapse all updates until next mainloop iteration...
	QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void PsiToolBar::update()
{
	d->dirty = false;
	clear();
	initialize(d->base, false);
}

void PsiToolBar::optionRemoved(const QString& option)
{
	if (d->base == option || d->base.startsWith(option + ".")) {
		deleteLater();
	}
}


// bool PsiToolBar::isCustomizeable() const
// {
// 	return d->customizeable;
// }
//
// void PsiToolBar::setCustomizeable( bool b )
// {
// 	d->customizeable = b;
// }
//
// bool PsiToolBar::isMoveable() const
// {
// 	return d->moveable;
// }
//
// void PsiToolBar::setMoveable( bool b )
// {
// 	d->moveable = b;
// }

void PsiToolBar::contextMenuEvent(QContextMenuEvent *e)
{
	e->accept();

	if (!d->customizeable)
		return;

	Q3PopupMenu pm;
	pm.insertItem(IconsetFactory::icon("psi/toolbars").icon(), tr("Configure &Toolbar..."), 0);

	int ret = pm.exec(e->globalPos());

	if (ret == 0) {
		d->psi->doToolbars();
	}
}

PsiActionList::ActionsType PsiToolBar::type() const
{
	return d->type;
}

void PsiToolBar::setType(PsiActionList::ActionsType type)
{
	d->type = PsiActionList::ActionsType(PsiActionList::ActionsType(type) | PsiActionList::Actions_Common);
}

void PsiToolBar::initialize(QString base, bool createUniqueActions)
{
	d->base = base;
	d->uniqueActions.clear();

// PsiOptions::instance()->getOption(base + ".").toString()

	setHorizontallyStretchable(PsiOptions::instance()->getOption(base + ".stretchable").toBool());
	setVerticallyStretchable(PsiOptions::instance()->getOption(base + ".stretchable").toBool());

	setMovingEnabled(!PsiOptions::instance()->getOption(base + ".locked").toBool());

	if (d->psi) {
		ActionList actions = d->psi->actionList()->suitableActions(d->type);
		QStringList keys = PsiOptions::instance()->getOption(base + ".actions").toStringList();
		for (int j = 0; j < keys.size(); j++) {
			IconAction *action = actions.action(keys[j]);

			if (action && action->isSeparator()) {
				addSeparator();
			}
			else if (action) {
				if (createUniqueActions) {
					action = action->copy();
					d->uniqueActions.append(action);
				}

				action->addTo(this);
				emit registerAction(action);
			}
			else {
				qWarning("PsiToolBar::initialize(): action %s not found!", keys[j].latin1());
			}
		}
	}
	else {
		qWarning("PsiToolBar::initialize(): psi is NULL!");
	}

	if (PsiOptions::instance()->getOption(base + ".visible").toBool()) {
		show();
	}
	else {
		hide();
	}
}

void PsiToolBar::structToOptions(const QString &base, ToolbarPrefs *tb)
{
	PsiOptions *o = PsiOptions::instance();
	o->setOption(base + ".name", tb->name);
	o->setOption(base + ".visible", tb->on);
	o->setOption(base + ".locked", tb->locked);
	o->setOption(base + ".stretchable", tb->stretchable);
	o->setOption(base + ".actions", tb->keys);
	o->setOption(base + ".dock.position", tb->dock);
	o->setOption(base + ".dock.index", tb->index);
	o->setOption(base + ".dock.nl", tb->nl);
	o->setOption(base + ".dock.extra-offset", tb->extraOffset);
}
