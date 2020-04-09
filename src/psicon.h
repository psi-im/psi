/*
 * psicon.h - core of Psi
 * Copyright (C) 2001-2002  Justin Karneges
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PSICON_H
#define PSICON_H

#include "profiles.h"
#include "psiactions.h"
#include "psievent.h"
#include "tabbablewidget.h"
#include "tunecontrollermanager.h"

#include <QList>
#include <functional>

class AccountsComboBox;
class AlertManager;
class AutoUpdater;
class ChatDlg;
class ContactUpdatesManager;
class ContactView;
class EDB;
class EDBItem;
class EventDlg;
class FileSharingManager;
class FileTransDlg;
class IconSelectPopup;
class NetworkAccessManager;
class PopupManager;
class ProxyManager;
class PsiAccount;
class PsiActionList;
class PsiCon;
class PsiContactList;
class PsiThemeManager;
class PsiToolBar;
class QMenuBar;
class QThread;
class TabDlg;
class TabManager;
class TuneController;
class UserListItem;
class WebServer;

namespace OpenPGP {
class Engine;
}

namespace XMPP {
class Jid;
}
using namespace XMPP;

class PsiCon : public QObject {
    Q_OBJECT
public:
    enum { QuitProgram, QuitProfile };
    PsiCon();
    ~PsiCon();

    bool init();
    void deinit();
    void gracefulDeinit(std::function<void()> callback);

    PsiContactList *       contactList() const;
    EDB *                  edb() const;
    TuneControllerManager *tuneManager() const;
    FileTransDlg *         ftdlg();
    TabManager *           tabManager() const;
    NetworkAccessManager * networkAccessManager() const;
    FileSharingManager *   fileSharingManager() const;
    PsiThemeManager *      themeManager() const;
    WebServer *            webServer() const;

    AlertManager *alertManager() const;

    QWidget *dialogFind(const char *className);
    void     dialogRegister(QWidget *w);
    void     dialogUnregister(QWidget *w);
    int      idle() const;

    QMenuBar *defaultMenuBar() const;

    ContactUpdatesManager *contactUpdatesManager() const;

    PsiAccount *createAccount(const QString &name, const Jid &j = "", const QString &pass = "", bool opt_host = false,
                              const QString &host = "", int port = 5222, bool legacy_ssl_probe = false,
                              UserAccount::SSLFlag ssl = UserAccount::SSL_Auto, QString proxy = "",
                              const QString &tlsOverrideDomain = "", const QByteArray &tlsOverrideCert = QByteArray());
    PsiAccount *createAccount(const UserAccount &);
    // void createAccount(const QString &, const QString &host="", int port=5222, bool ssl=false, const QString
    // &user="", const QString &pass="");
    void removeAccount(PsiAccount *);

    void playSound(const QString &);
    bool mainWinVisible() const;

    AccountsComboBox *accountsComboBox(QWidget *parent = nullptr, bool online_only = false);

    QStringList        recentGCList() const;
    void               recentGCAdd(const QString &);
    QStringList        recentBrowseList() const;
    void               recentBrowseAdd(const QString &);
    const QStringList &recentNodeList() const;
    void               recentNodeAdd(const QString &);

    EventDlg *createMessageDlg(const QString &, PsiAccount *);
    EventDlg *createEventDlg(const QString &, PsiAccount *);
    void      updateContactGlobal(PsiAccount *, const Jid &);

    PsiActionList *actionList() const;

    IconSelectPopup *iconSelectPopup() const;
    bool             filterEvent(const PsiAccount *, const PsiEvent::Ptr &) const;
    void             processEvent(const PsiEvent::Ptr &, ActivationType activationType);
    void             removeEvent(const PsiEvent::Ptr &e);

    Status::Type currentStatusType() const;
    QString      currentStatusMessage() const;

    bool haveAutoUpdater() const;
    void updateStatusPresets();

    PopupManager *popupManager() const;
    QStringList   xmppFatures() const;

signals:
    void quit(int);
    void accountAdded(PsiAccount *);
    void accountUpdated(PsiAccount *);
    void accountRemoved(PsiAccount *);
    void accountCountChanged();
    void accountActivityChanged();
    void emitOptionsUpdate();
    void restoringSavedChatsChanged();
    void statusMessageChanged(QString);
    void statusPresetsChanged();

public slots:
    void setGlobalStatus(const XMPP::Status &, bool withPriority = false, bool isManualStatus = false);
    void showStatusDialog(const QString &presetName);
    void doToolbars();
    void doStatusPresets();
    void checkAccountsEmpty();
    void setAccountsOrder(QList<PsiAccount *>);

public slots:
    void doSleep();
    void doWakeup();
    void closeProgram();
    void changeProfile();
    void doGroupChat();
    void doNewBlankMessage();
    void doOptions();
    void doFileTransDlg();
    void statusMenuChanged(XMPP::Status::Type, bool forceDialog = false);
    void setStatusMessage(QString message);
    void pa_updatedActivity();
    void pa_updatedAccount();
    void slotApplyOptions();
    void queueChanged();
    void recvNextEvent();
    void setStatusFromCommandline(const QString &status, const QString &message);
    void proxy_settingsChanged();
    void updateMainwinStatus();
    void openUri(const QString &uri);
    void openUri(const QUrl &uri);
    void openAtStyleUri(const QUrl &uri);
    void raiseMainwin();

private slots:
    void saveAccounts();
    void optionChanged(const QString &option);
    void forceSavePreferences(QSessionManager &);
    void startBounce();
    void aboutToQuit();
    void secondsIdle(int);
    void proceedWithSleep();
    void networkSessionOpened();

private:
    class Private;
    Private *d;
    friend class Private;
    ContactUpdatesManager *contactUpdatesManager_;

    void initNetSession();
    void deleteAllDialogs();
    void setShortcuts();

    friend class PsiAccount; // FIXME
    void    promptUserToCreateAccount();
    QString optionsFile() const;
    void    doQuit(int);
};

#endif // PSICON_H
