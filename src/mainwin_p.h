/*
 * mainwin_p.h - classes used privately by the main window.
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
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

#ifndef MAINWIN_P_H

#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QLayout>
#include <QLabel>
#include <QAction>
#include <QMouseEvent>

#include "psicon.h"
#include "iconaction.h"

class QMenu;

class SeparatorAction : public IconAction
{
	Q_OBJECT
public:
	SeparatorAction(QObject *parent, const char *name = 0);
	~SeparatorAction();

	virtual bool addTo(QWidget *w);

	virtual IconAction *copy() const;

private:
	class Private;
	Private *d;
};

class SpacerAction : public IconAction
{
	Q_OBJECT
public:
	SpacerAction(QObject *parent, const char *name = 0);
	~SpacerAction();

	virtual bool addTo (QWidget *w);

	virtual IconAction *copy() const;
};

class EventNotifierAction : public IconAction
{
	Q_OBJECT
public:
	EventNotifierAction(QObject *parent, const char *name = 0);
	~EventNotifierAction();

	void setMessage(const QString &);
	bool addTo (QWidget *w);

	void hide();
	void show();
	void updateVisibility();

	virtual IconAction *copy() const;
	virtual EventNotifierAction &operator=( const EventNotifierAction & );

signals:
	void clicked(int);

private slots:
	void objectDestroyed ();

private:
	class Private;
	Private *d;
};

class PopupAction : public IconAction
{
	Q_OBJECT
private:
	class Private;
	Private *d;

private slots:
	void objectDestroyed ();

public slots:
	void setEnabled (bool);

public:
	PopupAction (const QString &label, QMenu *_menu, QObject *parent, const char *name);
	void setSizePolicy (const QSizePolicy &p);
	void setAlert (const PsiIcon *);
	void setIcon (const PsiIcon *, bool showText = true, bool alert = false);
	void setText (const QString &text);
	bool addTo (QWidget *w);

	virtual IconAction *copy() const;
	virtual PopupAction &operator=( const PopupAction & );
};

class MLabel : public QLabel
{
	Q_OBJECT
public:
	MLabel(QWidget *parent=0, const char *name=0);

protected:
	// reimplemented
	void mouseReleaseEvent(QMouseEvent *);
	void mouseDoubleClickEvent(QMouseEvent *);

signals:
	void clicked(int);
	void doubleClicked();
};

class MAction : public IconActionGroup
{
	Q_OBJECT

public:
	MAction(PsiIcon, const QString &, int id, PsiCon *, QObject *parent);
	MAction(const QString &, int id, PsiCon *, QObject *parent);

	// reimplemented
	virtual bool addTo(QWidget *);

	virtual IconAction *copy() const;
	virtual MAction &operator=( const MAction & );

signals:
	void activated(PsiAccount *, int);

private slots:
	void numAccountsChanged();
	void actionActivated();
	void slotActivated();

protected:
	// reimplemented
	virtual void doSetMenu(QMenu* menu);

private:
	int id_;
	PsiCon* controller_;

	void init(const QString& name, PsiIcon, int id, PsiCon* psi);
	QList<PsiAccount*> accounts() const;
};

#endif

