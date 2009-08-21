/*
 * actionlineedit.h - QLineEdit widget with buttons on right side
 * Copyright (C) 2009 Sergey Il'inykh
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#ifndef ACTIONLINEEDIT_H
#define ACTIONLINEEDIT_H

#include <QLineEdit>
#include <QAbstractButton>
#include <QList>
#include <QAction>

class ActionLineEditButton : public QAbstractButton
{
	Q_OBJECT

public:
	ActionLineEditButton( QWidget *parent );
	void setDefaultAction(QAction *);
	QAction * defaultAction();
	void setPopup(QWidget *);

public slots:
	void updateFromAction(); //FIXME private
	void showPopup();

protected:
	void paintEvent ( QPaintEvent * event );
	QSize sizeHint () const;

private:
	QAction *action_;
	QWidget *popup_;
};

class ActionLineEdit : public QLineEdit
{
	Q_OBJECT
	Q_PROPERTY(Qt::ToolButtonStyle toolButtonStyle READ toolButtonStyle WRITE setToolButtonStyle)
	Q_ENUMS(Qt::ToolButtonStyle)

public:
	ActionLineEdit( QWidget *parent );
	ActionLineEditButton * widgetForAction ( QAction * action );
	void actionEvent ( QActionEvent * event );
	Qt::ToolButtonStyle toolButtonStyle() const { return toolButtonStyle_; }
	void setToolButtonStyle(const Qt::ToolButtonStyle newStyle) { toolButtonStyle_ = newStyle; }

private:
	Qt::ToolButtonStyle toolButtonStyle_;
};

#endif // ACTIONLINEEDIT_H
