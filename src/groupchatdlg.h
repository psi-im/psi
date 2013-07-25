/*
 * groupchatdlg.h - dialogs for handling groupchat
 * Copyright (C) 2001, 2002  Justin Karneges
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

#ifndef GROUPCHATDLG_H
#define GROUPCHATDLG_H

#include <QWidget>
#include <QDialog>


#include "advwidget.h"
#include "tabbablewidget.h"

#include "ui_groupchatdlg.h"
#include "mucmanager.h"
#include "advwidget.h"

using namespace XMPP;

class PsiCon;
class PsiAccount;
class PsiOptions;
class QRect;
class GCMainDlg;
class QColorGroup;
namespace XMPP {
	class Message;
}

/*class GCLineEdit : public QLineEdit
{
	Q_OBJECT
public:
	GCLineEdit(QWidget *parent=0, const char *name=0);

signals:
	void upPressed();
	void downPressed();

protected:
	void keyPressEvent(QKeyEvent *);
};*/

class GCMainDlg : public TabbableWidget
{
	Q_OBJECT
public:
	GCMainDlg(PsiAccount *, const Jid &, TabManager *tabManager);
	~GCMainDlg();

	PsiAccount* account() const;

	void error(int, const QString &);
	void presence(const QString &, const Status &);
	void message(const Message &);
	void joined();
	void setPassword(const QString&);
	const QString& nick() const;

	bool isInactive() const;
	void reactivate();

	// reimplemented
	virtual TabbableWidget::State state() const;
	virtual int unreadMessageCount() const;
	virtual QString desiredCaption() const;

protected:
	void setShortcuts();

	// reimplemented
	void dragEnterEvent(QDragEnterEvent *);
	void dropEvent(QDropEvent *);
	void closeEvent(QCloseEvent *);
	void mucInfoDialog(const QString& title, const QString& message, const Jid& actor, const QString& reason);

signals:
	void aSend(const Message &);

public slots:
	// reimplemented
	virtual void deactivated();
	virtual void activated();
	virtual void ensureTabbedCorrectly();

	void optionsUpdate();

private slots:
	void scrollUp();
	void scrollDown();
	void mle_returnPressed();
	void doTopic();
	void openFind();
	void configureRoom();
	void doFind(const QString &);
	void pa_updatedActivity();
	void goDisc();
	void goConn();
	void goForcedLeave();
	void lv_action(const QString &, const Status &, int);
	void doClear();
	void doClearButton();
	void buildMenu();
	void logSelectionChanged();
	void setConnecting();
	void unsetConnecting();
	void action_error(MUCManager::Action, int, const QString&);
	void updateIdentityVisibility();
#ifdef WHITEBOARDING
	void openWhiteboard();
#endif
	void chatEditCreated();

public:
	class Private;
	friend class Private;
private:
	Private *d;
	Ui::GroupChatDlg ui_;

	void doAlert();
	void appendSysMsg(const QString &, bool, const QDateTime &ts=QDateTime(), bool prepareAsChatMessage=false);
	void appendMessage(const Message &, bool);
	void updateLastMsgTime(QDateTime t);
	void setLooks();

	void mucKickMsgHelper(const QString &nick, const Status &s, const QString &nickJid, const QString &title,
			const QString &youSimple, const QString &youBy, const QString &someoneSimple,
			const QString &someoneBy);

	void contextMenuEvent(QContextMenuEvent *);

	QString getNickColor(QString);
	QMap<QString,int> nicks;
	int nicknumber;
	PsiOptions* options_;
	QDateTime lastMsgTime_;
};

class GCFindDlg : public QDialog
{
	Q_OBJECT
public:
	GCFindDlg(const QString&, QWidget* parent);
	~GCFindDlg();

	void found();
	void error(const QString &);

signals:
	void find(const QString &);

private slots:
	void doFind();

private:
	QLineEdit *le_input;
};

#endif
