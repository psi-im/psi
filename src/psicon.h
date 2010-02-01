/*
 * psicon.h - core of Psi
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

#ifndef PSICON_H
#define PSICON_H

#include <QList>

#include "profiles.h"
#include "psiactions.h"
#include "psievent.h"
#include "tabbablewidget.h"

using namespace XMPP;

class PsiCon;
class PsiAccount;
class PsiEvent;
class ContactView;
class AutoUpdater;
class EventDlg;
class UserListItem;
class EDB;
class EDBItem;
class ProxyManager;
class QMenuBar;
class FileTransDlg;
class IconSelectPopup;
class QThread;
class PsiActionList;
class PsiToolBar;
class TabDlg;
class AccountsComboBox;
class ChatDlg;
class AlertManager;
class TuneController;
class PsiContactList;
class TabManager;
class ContactUpdatesManager;

namespace OpenPGP {
	class Engine;
}
namespace XMPP {
	class Jid;
}

class PsiCon : public QObject
{
	Q_OBJECT
public:
	enum { QuitProgram, QuitProfile };
	PsiCon();
	~PsiCon();

	bool init();
	void deinit();

	PsiContactList* contactList() const;
#ifndef NEWCONTACTLIST
	ContactView* contactView() const;
#endif
	EDB *edb() const;
	TuneController* tuneController() const;
	ProxyManager *proxy() const;
	FileTransDlg *ftdlg() const;
	TabManager *tabManager() const;

	AlertManager *alertManager() const;

	QWidget *dialogFind(const char *className);
	void dialogRegister(QWidget *w);
	void dialogUnregister(QWidget *w);

	QMenuBar* defaultMenuBar() const;

	ContactUpdatesManager* contactUpdatesManager() const;

	PsiAccount* createAccount(const QString &name, const Jid &j="", const QString &pass="", bool opt_host=false, const QString &host="", int port=5222, bool legacy_ssl_probe = true, UserAccount::SSLFlag ssl=UserAccount::SSL_Auto, QString proxy="", const QString &tlsOverrideDomain="", const QByteArray &tlsOverrideCert=QByteArray());
	PsiAccount *createAccount(const UserAccount &);
	//void createAccount(const QString &, const QString &host="", int port=5222, bool ssl=false, const QString &user="", const QString &pass="");
	void removeAccount(PsiAccount *);

	void playSound(const QString &);
	bool mainWinVisible() const;

	AccountsComboBox *accountsComboBox(QWidget *parent=0, bool online_only = false);

	QStringList recentGCList() const;
	void recentGCAdd(const QString &);
	QStringList recentBrowseList() const;
	void recentBrowseAdd(const QString &);
	const QStringList & recentNodeList() const;
	void recentNodeAdd(const QString &);

	EventDlg *createEventDlg(const QString &, PsiAccount*);
	void updateContactGlobal(PsiAccount *, const Jid &);

	PsiActionList *actionList() const;

	IconSelectPopup *iconSelectPopup() const;
	bool filterEvent(const PsiAccount*, const PsiEvent*) const;
	void processEvent(PsiEvent*, ActivationType activationType);

	Status::Type currentStatusType() const;
	Status::Type lastLoggedInStatusType() const;
	QString currentStatusMessage() const;

	bool haveAutoUpdater() const;

signals:
	void quit(int);
	void accountAdded(PsiAccount *);
	void accountUpdated(PsiAccount *);
	void accountRemoved(PsiAccount *);
	void accountCountChanged();
	void accountActivityChanged();
	void emitOptionsUpdate();
	void restoringSavedChatsChanged();

public slots:
	void setGlobalStatus(const Status &, bool withPriority = false, bool isManualStatus = false);
	void doToolbars();
	void checkAccountsEmpty();

public slots:
	void doSleep();
	void doWakeup();
	void closeProgram();
	void changeProfile();
	void doManageAccounts();
	void doGroupChat();
	void doNewBlankMessage();
	void doOptions();
	void doFileTransDlg();
	void statusMenuChanged(int);
	void pa_updatedActivity();
	void pa_updatedAccount();
	void slotApplyOptions();
	void queueChanged();
	void recvNextEvent();
	void setStatusFromDialog(const XMPP::Status &, bool withPriority, bool isManualStatus);
	void setStatusFromCommandline(const QString &status, const QString &message);
	void proxy_settingsChanged();
	void updateMainwinStatus();
	void openUri(const QString &uri);
	void openUri(const QUrl &uri);
	void openAtStyleUri(const QUrl &uri);
	void raiseMainwin();

private slots:
	void saveAccounts();
	void saveCapabilities();
	void optionChanged(const QString& option);
	void forceSavePreferences();
	void startBounce();
	void aboutToQuit();

private:
	class Private;
	Private *d;
	friend class Private;
	ContactUpdatesManager* contactUpdatesManager_;

	void deleteAllDialogs();
	void s5b_init();
	void updateS5BServerAddresses();
	void setShortcuts();

	friend class PsiAccount; // FIXME
	void promptUserToCreateAccount();
	QString optionsFile() const;
	void doQuit(int);

	void registerCaps(const QString& ext, const QStringList& features);
};

#endif
