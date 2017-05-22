/*
 * psicontactmenu.cpp - a PsiContact context menu
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#pragma once

#include <QMenu>
#include <QPointer>

class PsiContact;

class GroupMenu : public QMenu
{
	Q_OBJECT
public:
	GroupMenu(QWidget* parent);
	void updateMenu(PsiContact* contact);

signals:
	void groupActivated(QString groupName);

private:
	QPointer<PsiContact> contact_;
	QAction* createNewGroupAction_;

	/**
	 * \param text will be shown on screen, and \param groupName is the
	 * actual group name. Specify true as \param current when group is
	 * currently selected for a contact.
	 */
	void addGroup(QString text, QString groupName, bool selected);

private slots:
	void actionActivated();
	void createNewGroup();
};
