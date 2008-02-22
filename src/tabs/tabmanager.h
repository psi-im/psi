/*
 * tabmanager.h - Controller for tab dialogs.
 * Copyright (C) 2007  Kevin Smith
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
#ifndef _TABMANAGER_H_
#define _TABMANAGER_H_

#include <QObject>
#include <QList>
#include "psicon.h"

class TabbableWidget;
class TabDlg;
class TabDlgDelegate;

class TabManager : public QObject
{
	Q_OBJECT
public:
	TabManager(PsiCon *psiCon, QObject *parent = 0);
	~TabManager();

	PsiCon* psiCon() const;

	/**
	 * Get the default tabset for this widget (created if needed).
	 */ 
	TabDlg* getTabs(QWidget *widget);
	
	/**
	 * Return a new tabset (for this widget).
	 */ 
	TabDlg*	newTabs(QWidget *widget=0);
	
	/**
	 * Checks if a tabset manages this widget.
	 */ 
	bool isChatTabbed(const TabbableWidget*) const;
	
	/**
	 * Returns the tab dialog that owns the supplied widget.
	 */
	TabDlg* getManagingTabs(const TabbableWidget*) const;
	
	/**
	 * Returns all active tabsets (could be empty).
	 */ 
	const QList<TabDlg*>& tabSets();
	
	/**
	 * Checks if a given widget should be in a tabset 
	 * (depends on set options and widget type).
	 */
	bool shouldBeTabbed(QWidget *widget); 

	/**
	 * removes and deletes all tabsets
	 */
	void deleteAll();

	
	/**
	 * Returns the Kind of the given widget.
	 */
	QChar tabKind(QWidget *widget);
	
	/**
	 * return the preferred tabset for a given kind of tabs(0 for none).
	 */ 
	TabDlg *preferredTabsForKind(QChar kind);
	
	/**
	 * set the preferred tabset for a given kind of tabs
	 */
	void setPreferredTabsForKind(QChar kind, TabDlg *tabs);

	/**
	 * set the delegate to be used for all created TabDlgs
	 */
	void setTabDlgDelegate(TabDlgDelegate *delegate);

	/**
	 * enable/disable user dragging/detach/assignment of tabs
	 *
	 * the default is enabled
	 */
	void setUserManagementEnabled(bool enabled);

	/**
	 * enable/disable display of PsiTabBar when there is only one tab
	 *
	 * the default is enabled
	 */
	void setTabBarShownForSingles(bool enabled);

	/**
	 * enable/disable simplified caption mode
	 *
	 * the default is disabled
	 */
	void setSimplifiedCaptionEnabled(bool enabled);

public slots:
	void tabDestroyed(QObject*);
	void tabResized(QSize);

private:
	QMap<QChar, TabDlg*> preferedTabsetForKind_;
	QMap<TabDlg*, QString> tabsetToKinds_;
 	QList<TabDlg*> tabs_;
	QList<TabbableWidget*> tabControlledChats_;
	PsiCon *psiCon_;
	TabDlgDelegate *tabDlgDelegate_;
	bool userManagement_;
	bool tabSingles_;
	bool simplifiedCaption_;
};

#endif /* _TABMANAGER_H_ */
