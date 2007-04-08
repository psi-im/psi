/*
 * eventdlg.h - dialog for sending / receiving messages and events
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

#ifndef EVENTDLG_H
#define EVENTDLG_H

#include <qlineedit.h>
#include <q3listview.h>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QDropEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <Q3PopupMenu>
#include <QDragEnterEvent>

#include "userlist.h"
#include "ui_addurl.h"
#include "advwidget.h"

class QDateTime;
class QStringList;
class PsiEvent;
class PsiCon;
class PsiAccount;
class Icon;
class EventDlg;
namespace XMPP {
	class Jid;
}

using namespace XMPP;

class ELineEdit : public QLineEdit
{
	Q_OBJECT
public:
	ELineEdit(EventDlg *parent, const char *name=0);

signals:
	void changeResource(const QString &);
	void tryComplete();

protected:
	// reimplemented
	void dragEnterEvent(QDragEnterEvent *);
	void dropEvent(QDropEvent *);
	void keyPressEvent(QKeyEvent *);
	Q3PopupMenu *createPopupMenu();

private slots:
	void resourceMenuActivated(int);

private:
	UserResourceList url;
};

class AttachView : public Q3ListView
{
	Q_OBJECT
public:
	AttachView(QWidget *parent=0, const char *name=0);
	~AttachView();

	void setReadOnly(bool);
	void urlAdd(const QString &, const QString &);
	void gcAdd(const QString &, const QString& = QString(), const QString& = QString(), const QString& = QString());

	UrlList urlList() const;
	void addUrlList(const UrlList &);

signals:
	void childCountChanged();
	void actionGCJoin(const QString &, const QString&);

private slots:
	void qlv_context(Q3ListViewItem *, const QPoint &, int);
	void qlv_doubleClicked(Q3ListViewItem *);

private:
	bool v_readOnly;

	void goURL(const QString &);
};

class AddUrlDlg : public QDialog, public Ui::AddUrl
{
	Q_OBJECT
public:
	AddUrlDlg(QWidget *parent=0);
	~AddUrlDlg();
};

class EventDlg : public AdvancedWidget<QWidget>
{
	Q_OBJECT
public:
	// compose
	EventDlg(const QString &, PsiCon *, PsiAccount *);
	// read
	EventDlg(const Jid &, PsiAccount *, bool unique);
	~EventDlg();

	QString text() const;
	void setHtml(const QString &);
	void setSubject(const QString &);
	void setThread(const QString &);
	void setUrlOnShow();

	PsiAccount *psiAccount();

	static QSize defaultSize();

signals:
	void aChat(const Jid& jid);
	void aReply(const Jid &jid, const QString &body, const QString &subject, const QString &thread);
	void aReadNext(const Jid &);
	void aDeny(const Jid &);
	void aAuth(const Jid &);
	void aRosterExchange(const RosterExchangeItems &);

protected:
	// reimplemented
	void showEvent(QShowEvent *);
	void resizeEvent(QResizeEvent *);
	void keyPressEvent(QKeyEvent *);
	void closeEvent(QCloseEvent *);

public slots:
	void optionsUpdate();
	void closeAfterReply();
	void updateContact(const Jid &);
	void updateEvent(PsiEvent *);
	void updateReadNext(Icon *, int);
	void actionGCJoin(const QString &, const QString&);

private slots:
	void to_textChanged(const QString &);
	void to_changeResource(const QString &);
	void to_tryComplete();
	void updateIdentity(PsiAccount *);
	void updateIdentityVisibility();
	void accountUpdatedActivity();
	void doWhois(bool force=false);
	void doSend();
	void doReadNext();
	void doChat();
	void doReply();
	void doQuote();
	void doDeny();
	void doAuth();
	void doInfo();
	void doHistory();
	void showHideAttachView();
	void addUrl();

	void updatePGP();
	void encryptedMessageSent(int, bool, int);
	void trySendEncryptedNext();

public:
	class Private;
private:
	Private *d;

	void doneSend();

	void init();
	QStringList stringToList(const QString &, bool enc=true) const;
	QString findJidInString(const QString &) const;
	QString expandAddresses(const QString &, bool enc=true) const;
	void buildCompletionList();
	void setAccount(PsiAccount *);
	void setTime(const QDateTime &, bool late=false);

	friend class ELineEdit;
	UserResourceList getResources(const QString &) const;
	QString jidToString(const Jid &, const QString &r="") const;
};

#endif
