/*
 * rosteravatarframe.h
 * Copyright (C) 2010  Evgeny Khryukin
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

#ifndef ROSTERAVATARFRAME_H
#define ROSTERAVATARFRAME_H

#include "ui_rosteravatarframe.h"

#include <QKeyEvent>

class RosterAvatarFrame : public QFrame
{
	Q_OBJECT
	public:
		RosterAvatarFrame(QWidget *parent);
		void setAvatar(const QPixmap &avatar);
		void setStatusIcon(const QIcon &ico);
		void setNick(const QString &nick);
		void setStatusMenu(QMenu *menu);

	signals:
		void statusMessageChanged(QString);
		void setMood();
		void setActivity();

	protected:
		void keyPressEvent(QKeyEvent *event);

	private slots:
		void statusMessageReturnPressed();
		void optionChanged(QString);

	public slots:
		void setStatusMessage(const QString &message);
		void setMoodIcon(const QString &mood);
		void setActivityIcon(const QString &activity);

	private:
		Ui::RosterAvatarFrame ui_;
		QString statusMessage_;
		QMenu *statusMenu_;
		QPixmap avatarPixmap;

		void drawAvatar();
		void setFont();
};

#endif
