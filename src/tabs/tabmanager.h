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
#include <Q3PtrList>
#include "psicon.h"

class TabbableWidget;
class TabDlg;

class TabManager : public QObject
{
	Q_OBJECT
public:
	TabManager(PsiCon *psiCon, QObject *parent = 0);
	~TabManager();

	PsiCon* psiCon() const;

	/**
	 * Get the default tabdlg (created if needed).
	 */ 
	TabDlg* getTabs();
	
	/**
	 * Return a new tabset.
	 */ 
	TabDlg*	newTabs();
	
	/**
	 * Checks if a tabset manages this widget.
	 */ 
	bool isChatTabbed(const TabbableWidget*) const;
	
	/**
	 * Gets the tabbed widget with the specified jid.
	 */ 
	TabbableWidget* getChatInTabs(QString);
	
	/**
	 * Returns the tab dialog that owns the supplied widget.
	 */
	TabDlg* getManagingTabs(const TabbableWidget*) const;
	
	/**
	 * Returns all active tabsets (could be empty).
	 */ 
	Q3PtrList<TabDlg>* getTabSets();
	
	/**
	 * Checks if a given widget should be in a tabset 
	 * (depends on set options and widget type).
	 */
	bool shouldBeTabbed(QWidget *widget); 


	void deleteAll();

public slots:
	/**
	 * Called when a tab dialog is closed.
	 * Removes it from the list of possible tabdlgs.
	 */ 
	void tabDying(TabDlg*);

private:
 	Q3PtrList<TabDlg> tabs_;
	Q3PtrList<TabbableWidget> tabControlledChats_;
	PsiCon *psiCon_;
};

#endif /* _TABMANAGER_H_ */
