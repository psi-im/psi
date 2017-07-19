/*
 * tabbable.h
 * Copyright (C) 2007 Kevin Smith
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

#ifndef TABBABLE_H
#define TABBABLE_H

#include <QIcon>
#include "advwidget.h"
#include "im.h" // ChatState

namespace XMPP {
	class Jid;
	class Message;
}
using namespace XMPP;

class PsiAccount;
class TabManager;
class TabDlg;

class TabbableWidget : public AdvancedWidget<QWidget>
{
	Q_OBJECT
public:
	TabbableWidget(const Jid &, PsiAccount *, TabManager *tabManager);
	~TabbableWidget();

	PsiAccount* account() const;
	void setTabIcon(const QIcon &);
	const QIcon &icon() const;

	virtual Jid jid() const;
	virtual const QString & getDisplayName() const;

	virtual bool readyToHide();
	TabDlg* getManagingTabDlg();

	bool isTabbed();
	bool isActiveTab();

	// reimplemented
	virtual void doFlash(bool on);

	virtual void invalidateTab();

	enum State {
		StateNone = 0,
		StateComposing
	};
	virtual State state() const = 0;
	virtual int unreadMessageCount() const = 0;
	virtual QString desiredCaption() const = 0;

signals:
	void invalidateTabInfo();
	void updateFlashState();
	void eventsRead(const Jid &);

public slots:
	virtual void deactivated();
	virtual void activated();
	void bringToFront(bool raiseWindow = true);
	virtual void ensureTabbedCorrectly();
	void hideTab();

protected:
	virtual void setJid(const Jid&);

	// reimplemented
	void changeEvent(QEvent* e);

private:
	Jid jid_;
	PsiAccount *pa_;
	TabManager *tabManager_;
	QIcon icon_;
};

#endif
