/*
 * iconaction.h - the QAction subclass that uses Icons and supports animation
 * Copyright (C) 2003-2005  Michail Pishchagin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef ICONACTION_H
#define ICONACTION_H

#include <QAction>
#include <QList>

class QToolButton;
class QPixmap;
class QIcon;
class PsiIcon;
class IconToolButton;
class QChildEvent;

class IconAction : public QAction
{
	Q_OBJECT
public:
	IconAction(QObject *parent, const QString &name = QString());
	IconAction(const QString &statusTip, const QString &icon, const QString &text, QKeySequence accel, QObject *parent, const QString &name = QString(), bool checkable = false);
	IconAction(const QString &statusTip, const QString &icon, const QString &text, QList<QKeySequence> accel, QObject *parent, const QString &name = QString(), bool checkable = false);
	IconAction(const QString &statusTip, const QString &text, QKeySequence accel, QObject *parent, const QString &name = QString(), bool checkable = false);
	IconAction(const QString &statusTip, const QString &text, QList<QKeySequence> accel, QObject *parent, const QString &name = QString(), bool checkable = false);
	IconAction(const QString &text, QObject *parent, const QString &icon);
	~IconAction();

	virtual bool addTo(QWidget *);

	const PsiIcon *psiIcon() const;
	void setPsiIcon(const PsiIcon *);
	void setPsiIcon(const QString &);
	QString psiIconName() const;

	void setMenu( QMenu * );

	void setIcon( const QIcon & );
	void setVisible( bool );

	virtual IconAction *copy() const;
	virtual IconAction &operator=( const IconAction & );

	void setParent(QObject *newParent);

public slots:
	void setEnabled(bool);
	void setChecked(bool);
	void setText(const QString &);

protected:
	virtual void doSetMenu(QMenu* menu);
	virtual void addingToolButton(IconToolButton *) { }
	//virtual void addingMenuItem(QPopupMenu *, int id) { Q_UNUSED(id); }
	QList<IconToolButton *> buttonList();

	QString toolTipFromMenuText() const;

private slots:
	void objectDestroyed();
	void iconUpdated();
	void toolButtonToggled(bool);

public:
	class Private;
private:
	Private *d;
	friend class Private;
};

class IconActionGroup : public IconAction
{
	Q_OBJECT
public:
	IconActionGroup(QObject *parent, const char *name = 0, bool exclusive = false);
	~IconActionGroup();

	void setExclusive( bool );
	bool isExclusive() const;

	void add( QAction * );
	void addSeparator();

	bool addTo( QWidget * );

	void setUsesDropDown( bool );
	bool usesDropDown() const;

	void childEvent(QChildEvent *);

	void addingToolButton(IconToolButton *);

	IconAction *copy() const;

	QMenu* popup();

public:
	class Private;
private:
	Private *d;
	friend class Private;
};

#endif
