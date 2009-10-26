/*
 * Copyright (C) 2009  Barracuda Networks, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef GROUPCHATBROWSECONTROLLER_H
#define GROUPCHATBROWSECONTROLLER_H

#include <QWidget>
class PsiAccount;

class GroupChatBrowseController : public QObject
{
	Q_OBJECT

public:
	GroupChatBrowseController(PsiAccount *pa, QWidget *parentWindow = 0);
	~GroupChatBrowseController();

	// PsiAccount calls one of these during a join, just like MUCJoinDlg
	void joined();
	void error(int, const QString &);

private:
	class Private;
	friend class Private;
	Private *d;
};

#endif
