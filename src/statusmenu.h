/*
 * statusmenu.h - helper class that displays available statuses using QMenu
 * Copyright (C) 2008-2010  Michail Pishchagin
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

#ifndef STATUSMENU_H
#define STATUSMENU_H

#include <QMenu>

#include "xmpp_status.h"

class StatusMenu : public QMenu
{
	Q_OBJECT
public:
	StatusMenu(QWidget* parent);

	void setStatus(XMPP::Status::Type);

signals:
	void statusChanged(XMPP::Status::Type);

private:
	void addStatus(XMPP::Status::Type type);

private slots:
	void actionActivated();

private:
	XMPP::Status::Type currentStatus_;

	XMPP::Status::Type actionStatus(const QAction* action) const;
};

#endif
