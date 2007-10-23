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
#include "psicon.h"
#include "iconset.h"
#include "psicon.h"
#include "iconaction.h"
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
	QString group;
	int groupIndex;
	PsiCon *psi;
	Q3PtrList<IconAction> uniqueActions;
};

PsiToolBar::Private::Private()
{
	customizeable = true;
	moveable = true;
	psi = 0;
	uniqueActions.setAutoDelete( true );
}

//----------------------------------------------------------------------------
// PsiToolBar
//----------------------------------------------------------------------------

PsiToolBar::PsiToolBar(const QString& label, Q3MainWindow* mainWindow, PsiCon* psi)
: Q3ToolBar(label, mainWindow, (QWidget*)mainWindow)
{
	d = new Private();
	d->psi = psi;
}

PsiToolBar::~PsiToolBar()
{
	delete d;
}

bool PsiToolBar::isCustomizeable() const
{
	return d->customizeable;
}

void PsiToolBar::setCustomizeable( bool b )
{
	d->customizeable = b;
}

bool PsiToolBar::isMoveable() const
{
	return d->moveable;
}

void PsiToolBar::setMoveable( bool b )
{
	d->moveable = b;
}
	
void PsiToolBar::contextMenuEvent(QContextMenuEvent *e)
{
	e->accept();

	if ( !d->customizeable )
		return;

	Q3PopupMenu pm;
	pm.insertItem(IconsetFactory::icon("psi/toolbars").icon(), tr("Configure &Toolbar..."), 0);

	int ret = pm.exec( e->globalPos() );

	if ( ret == 0 ) {
		d->psi->doToolbars();
	}
}

PsiActionList::ActionsType PsiToolBar::type() const
{
	return d->type;
}

QString PsiToolBar::group() const
{
	return d->group;
}

int PsiToolBar::groupIndex() const
{
	return d->groupIndex;
}

void PsiToolBar::setGroup( QString group, int index )
{
	d->group = group;
	d->groupIndex = index;
}

void PsiToolBar::setType( PsiActionList::ActionsType type )
{
	d->type = PsiActionList::ActionsType( PsiActionList::ActionsType( type ) | PsiActionList::Actions_Common );
}

void PsiToolBar::initialize( Options::ToolbarPrefs &tbPref, bool createUniqueActions )
{
	d->uniqueActions.clear();

	setHorizontallyStretchable( tbPref.stretchable );
	setVerticallyStretchable( tbPref.stretchable );

	setMovingEnabled ( !tbPref.locked );

	if ( d->psi ) {
		ActionList actions = d->psi->actionList()->suitableActions( d->type );
		QStringList keys = tbPref.keys;
		for (int j = 0; j < keys.size(); j++) {
			IconAction *action = actions.action( keys[j] );

			if ( action && action->isSeparator() ) {
				addSeparator();
			}
			else if ( action ) {
				if ( createUniqueActions ) {
					action = action->copy();
					d->uniqueActions.append( action );
				}

				action->addTo( this );
				emit registerAction( action );
			}
			else
				qWarning("PsiToolBar::initialize(): action %s not found!", keys[j].latin1());
		}
	}
	else
		qWarning("PsiToolBar::initialize(): psi is NULL!");

	if ( tbPref.on )
		show();
	else
		hide();

	tbPref.dirty = false;
}
