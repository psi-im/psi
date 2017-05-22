/*
 * invitetogroupchatmenu.h - invite to groupchat context menu option
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
 *
 * This file is part of the WhoerIM project.
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

class PsiContact;
class PsiAccount;
class PsiCon;

class InviteToGroupChatMenu : public QMenu
{
	Q_OBJECT

public:
	InviteToGroupChatMenu(QWidget *parent = 0);
	void updateMenu(PsiContact *contact);

signals:
	void inviteToGroupchat(PsiAccount *account, const QString &groupChat);

private slots:
	void actionActivated();

private:
	PsiCon* controller_;
};
