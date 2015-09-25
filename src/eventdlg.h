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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef EVENTDLG_H
#define EVENTDLG_H

#include <QLineEdit>
#include <QListWidget>

#include "xmpp_url.h"
#include "xmpp_rosterx.h"
#include "ui_addurl.h"
#include "advwidget.h"
#include "userlist.h"
#include "psievent.h"

class QDateTime;
class QStringList;
class PsiCon;
class PsiAccount;
class PsiIcon;
class EventDlg;
namespace XMPP {
	class Jid;
	class XData;
}
class PsiHttpAuthRequest;

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
	//QMenu *createPopupMenu();

private slots:
	void resourceMenuActivated(QAction*);

private:
	UserResourceList url;
};

class AttachView : public QListWidget
{
	Q_OBJECT
public:
	AttachView(QWidget* parent);
	~AttachView();

	void setReadOnly(bool);
	void urlAdd(const QString &, const QString &);
	void gcAdd(const QString &, const QString& = QString(), const QString& = QString(), const QString& = QString());

	UrlList urlList() const;
	void addUrlList(const UrlList &);

signals:
	void childCountChanged();
	void actionGCJoin(const QString &, const QString&);

protected:
	// reimplemented
	void contextMenuEvent(QContextMenuEvent* e);

private slots:
	void qlv_doubleClicked(QListWidgetItem *);

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

	static bool messagingEnabled();

	QString text() const;
	bool isForAll() const;
	void setHtml(const QString &);
	void setSubject(const QString &);
	void setThread(const QString &);
	void setUrlOnShow();

	PsiAccount *psiAccount();

signals:
	void aChat(const Jid& jid);
	void aReply(const Jid &jid, const QString &body, const QString &subject, const QString &thread);
	void aReadNext(const Jid &);
	void aDeny(const Jid &);
	void aAuth(const Jid &);
	void aHttpConfirm(const PsiHttpAuthRequest &);
	void aHttpDeny(const PsiHttpAuthRequest &);
	void aRosterExchange(const RosterExchangeItems &);
	void aFormSubmit(const XData&, const QString&, const Jid&);
	void aFormCancel(const XData&, const QString&, const Jid&);

protected:
	// reimplemented
	void showEvent(QShowEvent *);
	void keyPressEvent(QKeyEvent *);
	void closeEvent(QCloseEvent *);

public slots:
	void optionsUpdate();
	void closeAfterReply();
	void updateContact(const Jid &);
	void updateEvent(const PsiEvent::Ptr &);
	void updateReadNext(PsiIcon *, int);
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
	void doHttpConfirm();
	void doHttpDeny();
	void doInfo();
	void doHistory();
	void showHideAttachView();
	void addUrl();
	void doFormSubmit();
	void doFormCancel();

	void updatePGP();
	void encryptedMessageSent(int, bool, int, const QString &);
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
