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

#include <Q3ListView>
#include <QLineEdit>
#include <QContextMenuEvent>
#include <QDialog>
#include <QKeyEvent>
#include <QCloseEvent>

#include "advwidget.h"
#include "im.h" // Message
#include "mucmanager.h"
#include "xmpp_muc.h"
#include "xmpp_status.h"
#include "xmpp_jid.h"

using namespace XMPP;

class PsiCon;
class PsiAccount;
class PsiOptions;
class QRect;
class GCMainDlg;
class QPainter;
class QColorGroup;
class Q3DragObject;
class GCUserView;
class GCUserViewGroupItem;

class GCUserViewItem : public QObject, public Q3ListViewItem
{
public:
	GCUserViewItem(GCUserViewGroupItem *);
	void paintFocus(QPainter *, const QColorGroup &, const QRect &);
	void paintBranches(QPainter *p, const QColorGroup &cg, int w, int, int h);

	Status s;
};

class GCUserViewGroupItem : public Q3ListViewItem
{
public:
	GCUserViewGroupItem(GCUserView *, const QString&, int);
	void paintFocus(QPainter *, const QColorGroup &, const QRect &);
	void paintBranches(QPainter *p, const QColorGroup &cg, int w, int, int h);
	void paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int alignment);
	int compare(Q3ListViewItem *i, int col, bool ascending ) const;
private:
	int key_;
};

class GCUserView : public Q3ListView
{
	Q_OBJECT
public:
	GCUserView(GCMainDlg*, QWidget *parent=0, const char *name=0);
	~GCUserView();

	Q3DragObject* dragObject();
	void clear();
	void updateAll();
	Q3ListViewItem *findEntry(const QString &);
	void updateEntry(const QString &, const Status &);
	void removeEntry(const QString &);
	QStringList nickList() const;
	
protected:
	enum Role { Moderator = 0, Participant = 1, Visitor = 2 };
	
	GCUserViewGroupItem* findGroup(XMPP::MUCItem::Role a) const;
	bool maybeTip(const QPoint &);
	bool event(QEvent* e);

signals:
	void action(const QString &, const Status &, int);

private slots:
	void qlv_doubleClicked(Q3ListViewItem *);
	void qlv_contextMenuRequested(Q3ListViewItem *, const QPoint &, int);

private:
	GCMainDlg* gcDlg_;
};

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

class GCMainDlg : public AdvancedWidget<QWidget>
{
	Q_OBJECT
public:
	GCMainDlg(PsiAccount *, const Jid &);
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

	void keyPressEvent(QKeyEvent *);
	void dragEnterEvent(QDragEnterEvent *);
	void dropEvent(QDropEvent *);
	void closeEvent(QCloseEvent *);
	void windowActivationChange(bool);
	void mucInfoDialog(const QString& title, const QString& message, const Jid& actor, const QString& reason);

signals:
	void aSend(const Message &);

public slots:
	void optionsUpdate();

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
	void toggleSmallChat();
	void buildMenu();
	void logSelectionChanged();
	void setConnecting();
	void unsetConnecting();
	void action_error(MUCManager::Action, int, const QString&);
	void updateIdentityVisibility();

public:
	class Private;
private:
	Private *d;

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
