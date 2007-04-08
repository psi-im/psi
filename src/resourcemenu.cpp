/*
 * resourcemenu.cpp - helper class for displaying contact's resources
 * Copyright (C) 2006  Michail Pishchagin
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

#include "resourcemenu.h"
#include "psiiconset.h"
#include "common.h"
#include "userlist.h"

/**
 * \class ResourceMenu
 * Helper class that displays available resources using QMenu.
 * Please note that PsiMacStyle references name of ResourceMenu.
 */

ResourceMenu::ResourceMenu(QWidget *parent)
	: QMenu(parent)
{
	// nothing here
}

/**
 * Helper function to add resource to the menu.
 */
void ResourceMenu::addResource(const UserResource &r, int id)
{
	addResource(makeSTATUS(r.status()), r.name(), id);
}

/**
 * Helper function to add resource to the menu.
 */
void ResourceMenu::addResource(int status, QString name, int id)
{
	QString rname = name;
	if(rname.isEmpty())
		rname = tr("[blank]");

	//rname += " (" + status2txt(status) + ")";

	insertItem(PsiIconset::instance()->status(status), rname, id);
}
