/*
 * tabbable.cpp
 * Copyright (C) 2007 Kevin Smith
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

#include "tabbable.h"

#include "jidutil.h"

#ifdef Q_WS_WIN
#include <windows.h>
#endif

//----------------------------------------------------------------------------
// Tabbable
//----------------------------------------------------------------------------

Tabbable::Tabbable(const Jid &jid, PsiAccount *pa)
	:AdvancedWidget<QWidget>(0)
{
	jid_ = jid;
	pa_ = pa;
}

Tabbable::~Tabbable()
{
}

/**
 * Runs any gumph necessary before hiding a tab.
 * (checking new messages, setting the autodelete, cancelling composing etc)
 * \return Tabbable is ready to be hidden.
 */
bool Tabbable::readyToHide()
{
	return true;
}

Jid Tabbable::jid() const
{
	return jid_;
}

const QString& Tabbable::getDisplayName()
{
	return jid_.user();
}

void Tabbable::activated()
{
}
