/*
 * mainwin.h - the main window.  holds contactlist and buttons.
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
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

#ifndef MAINWIN_H
#define MAINWIN_H

#include "advwidget.h"
#include "xmpp_status.h"

#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QString>
#include <QStringList>

class ContactView;
class GlobalStatusMenu;
class IconAction;
class PsiAccount;
class PsiCon;
class PsiIcon;
class PsiToolBar;
class PsiTrayIcon;
class QAction;
class QMenu;
class QMenuBar;
class QMimeData;
class QPixmap;
class QPoint;

namespace XMPP {
    class Status;
}
using namespace XMPP;

class MainWin : public AdvancedWidget<QMainWindow>
{
    Q_OBJECT
public:
    MainWin(bool onTop, bool asTool, PsiCon *);
    ~MainWin();

    void setWindowOpts(bool onTop, bool asTool);
    void setUseDock(bool);
    void setUseAvatarFrame(bool state);
    void reinitAutoHide();

    void buildToolbars();
    PsiTrayIcon *psiTrayIcon();

    // evil stuff! remove ASAP!!
    QStringList actionList;
    QMap<QString, QAction*> actions;

    PsiCon *psiCon() const;

protected:
    // reimplemented
    bool eventFilter(QObject* o, QEvent* e);
    void resizeEvent(QResizeEvent *e);
    void closeEvent(QCloseEvent *);
    void changeEvent(QEvent *event);
    void enterEvent(QEvent *e);
    void leaveEvent(QEvent *e);
    void keyPressEvent(QKeyEvent *);
    QMenuBar* mainMenuBar() const;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, MSG *, long *);
#endif

signals:
    void statusChanged(XMPP::Status::Type);
    void statusMessageChanged(QString);
    void changeProfile();
    void blankMessage();
    void closeProgram();
    void doOptions();
    void doToolbars();
    void doGroupChat();
    void doFileTransDlg();
    void accountInfo();
    void recvNextEvent();

private slots:
    void buildStatusMenu();
    void buildOptionsMenu();
    void buildTrayMenu();
    void buildMainMenu();
    void buildToolsMenu();

    void setTrayToolTip();

    void trayClicked(const QPoint &, int);
    void trayDoubleClicked();
    void trayShow();
    void trayHide();
    void trayHideShow();

    void doRecvNextEvent();
    void statusClicked(int);
    void try2tryCloseProgram();
    void tryCloseProgram();

    void numAccountsChanged();
    void accountFeaturesChanged();
    void activatedAccOption(PsiAccount *, int);

    void actReadmeActivated ();
    void actOnlineWikiActivated ();
    void actOnlineHomeActivated ();
    void actOnlineForumActivated ();
    void actJoinPsiMUCActivated();
    void actBugReportActivated ();
    void actAboutActivated ();
    void actAboutQtActivated ();
    void actAboutPsiMediaActivated ();
    void actPlaySoundsActivated (bool);
    void actPublishTuneActivated (bool);
    void actEnableGroupsActivated (bool);
    void actDiagQCAPluginActivated();
    void actDiagQCAKeyStoreActivated();
    void actChooseStatusActivated();
    void actReconnectActivated();
    void actSetMoodActivated();
    void actSetActivityActivated();
    void actSetGeolocActivated();
    void actActiveContacts();

    bool showDockMenu(const QPoint &);
    void dockActivated();

    void searchClearClicked();
    void searchTextEntered(QString const &text);
    void searchTextStarted(QString const &text);

    void registerAction( IconAction * );
    void splitterMoved();

    void doTrayToolTip(QObject *, QPoint);

    void nickChanged();
    void avatarChanged(const Jid &jid);

    void hideTimerTimeout();

    void optionChanged(const QString&);

public slots:
    void setWindowIcon(const QPixmap&);
    void showNoFocus();

    void decorateButton(int);
    void updateReadNext(PsiIcon *nextAnim, int nextAmount);

    void optionsUpdate();
    void setTrayToolTip(const XMPP::Status &, bool usePriority = false, bool isManualStatus = false);

    void toggleVisible(bool fromTray = false);

    void avcallConfig();

private:
    void buildGeneralMenu(QMenu *);
    QString numEventsString(int) const;

    bool askQuit();

    void updateCaption();
    void updateTray();

    void saveToolbarsState();
    void loadToolbarsState();

    void buildStatusMenu(GlobalStatusMenu *statusMenu);

private:
    class Private;
    Private *d;
    friend class Private;
    QList<PsiToolBar*> toolbars_;
};

#endif // MAINWIN_H
