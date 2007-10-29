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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
class QPainter;
class QColorGroup;
class Q3DragObject;
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

	Jid jid() const;
	PsiAccount* account() const;

	void error(int, const QString &);
	void presence(const QString &, const Status &);
	void message(const Message &);
	void joined();
	void setPassword(const QString&);
	const QString& nick() const;

protected:
	void setShortcuts();

	// reimplemented
	void keyPressEvent(QKeyEvent *);
	void dragEnterEvent(QDragEnterEvent *);
	void dropEvent(QDropEvent *);
	void closeEvent(QCloseEvent *);
	void resizeEvent(QResizeEvent*);

	void windowActivationChange(bool);
	void mucInfoDialog(const QString& title, const QString& message, const Jid& actor, const QString& reason);

signals:
	void aSend(const Message &);
	void captionChanged(QString);
	void unreadEventUpdate(int);

public slots:
	void optionsUpdate();
	virtual void activated();
	
private slots:
	void scrollUp();
	void scrollDown();
	void mle_returnPressed();
	void doTopic();
	void openFind();
	void configureRoom();
	void doFind(const QString &);
	void flashAnimate();
	void pa_updatedActivity();
	void goDisc();
	void goConn();
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

	void doFlash(bool);
	void doAlert();
	void updateCaption();
	void appendSysMsg(const QString &, bool, const QDateTime &ts=QDateTime());
	void appendMessage(const Message &, bool);
	void setLooks();

	void contextMenuEvent(QContextMenuEvent *);

	QString getNickColor(QString);
	QMap<QString,int> nicks;
	int nicknumber;
	PsiOptions* options_;
};

class GCFindDlg : public QDialog
{
	Q_OBJECT
public:
	GCFindDlg(const QString &, QWidget *parent=0, const char *name=0);
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
