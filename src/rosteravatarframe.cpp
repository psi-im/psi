/*
 * rosteravatarframe.cpp
 * Copyright (C) 2010  Khryukin Evgeny
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

#include "rosteravatarframe.h"
#include "psioptions.h"
#include "iconset.h"
#include "qpainter.h"


RosterAvatarFrame::RosterAvatarFrame(QWidget *parent)
	: QFrame(parent)
	, statusMessage_("")
{
	ui_.setupUi(this);
	layout()->setMargin(PsiOptions::instance()->getOption("options.ui.contactlist.roster-avatar-frame.avatar.margin").toInt());
	layout()->setSpacing(0);
	setMoodIcon("pep/mood");
	setActivityIcon("pep/activities");
	setFont();

	connect(ui_.le_status_text, SIGNAL(returnPressed()), this, SLOT(statusMessageReturnPressed()));
	connect(ui_.tb_mood, SIGNAL(pressed()), this, SIGNAL(setMood()));
	connect(ui_.tb_activity, SIGNAL(pressed()), this, SIGNAL(setActivity()));
	connect(PsiOptions::instance(), SIGNAL(optionChanged(QString)),this, SLOT(optionChanged(QString)));
}

void RosterAvatarFrame::setStatusMessage(const QString &message)
{
	ui_.le_status_text->setText(message);
	ui_.le_status_text->setCursorPosition(0);
	statusMessage_ = message;
}

void RosterAvatarFrame::setAvatar(const QPixmap &avatar)
{
	avatarPixmap = avatar;
	drawAvatar();
}

void RosterAvatarFrame::setMoodIcon(const QString &mood)
{
	ui_.tb_mood->setIcon(QIcon(IconsetFactory::iconPixmap(mood)));
}

void RosterAvatarFrame::setActivityIcon(const QString &activity)
{
	ui_.tb_activity->setIcon(QIcon(IconsetFactory::iconPixmap(activity)));
}

void RosterAvatarFrame::drawAvatar()
{
	int avSize = PsiOptions::instance()->getOption("options.ui.contactlist.roster-avatar-frame.avatar.size").toInt();
	QPixmap av = avatarPixmap;
	if(!av.isNull()) {
		int radius = PsiOptions::instance()->getOption("options.ui.contactlist.avatars.radius").toInt();
		if(!radius)
			av = av.scaled(avSize,avSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		else {
			avSize = qMax(avSize, radius*2);
			av = av.scaled(avSize, avSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			int w = av.width(), h = av.height();
			QPainterPath pp;
			pp.addRoundedRect(0, 0, w, h, radius, radius);
			QPixmap avatar_icon = QPixmap(w, h);
			avatar_icon.fill(QColor(0,0,0,0));
			QPainter mp(&avatar_icon);
			mp.setBackgroundMode(Qt::TransparentMode);
			mp.setRenderHints(QPainter::Antialiasing, true);
			mp.fillPath(pp, QBrush(av));
			av = avatar_icon;
		}
	}
	ui_.lb_avatar->setPixmap(av);
	ui_.lb_avatar->setFixedSize(avSize,avSize);
}

void RosterAvatarFrame::setStatusIcon(const QIcon &ico)
{
	ui_.tb_status->setIcon(ico);
}

void RosterAvatarFrame::setNick(const QString &nick)
{
	ui_.lb_nick->setText(nick);
}

void RosterAvatarFrame::setFont()
{
	QFont f;
	f.fromString(PsiOptions::instance()->getOption("options.ui.look.font.contactlist").toString());
	ui_.lb_nick->setFont(f);
	ui_.le_status_text->setFont(f);
}

void RosterAvatarFrame::statusMessageReturnPressed()
{
	statusMessage_ = ui_.le_status_text->text();
	ui_.le_status_text->setCursorPosition(0);
	ui_.lb_nick->setFocus(); //remove focus from lineedit
	emit statusMessageChanged(statusMessage_);
}

void RosterAvatarFrame::setStatusMenu(QMenu *menu)
{
	statusMenu_ = menu;
	ui_.tb_status->setMenu(statusMenu_);
	ui_.tb_status->setPopupMode(QToolButton::InstantPopup);
}

void RosterAvatarFrame::keyPressEvent(QKeyEvent *e)
{
	if(e->key() == Qt::Key_Escape) {
		ui_.le_status_text->setText(statusMessage_);
		ui_.le_status_text->setCursorPosition(0);
		ui_.lb_nick->setFocus(); //remove focus from lineedit
		e->accept();
	}
	else {
		e->ignore();
	}
}

void RosterAvatarFrame::optionChanged(QString option)
{
	if(option == "options.ui.contactlist.avatars.radius" or "options.ui.contactlist.roster-avatar-frame.avatar.size")
		drawAvatar();
	if (option == "options.ui.look.font.contactlist")
		setFont();
	if (option == "options.ui.contactlist.roster-avatar-frame.avatar.margin")
		layout()->setMargin(PsiOptions::instance()->getOption("options.ui.contactlist.roster-avatar-frame.avatar.margin").toInt());
}
