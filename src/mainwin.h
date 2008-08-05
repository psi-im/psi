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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef MAINWIN_H
#define MAINWIN_H

#include <QMainWindow>
#include <QMap>
#include <QList>
#include <QString>
#include <QStringList>

#include "advwidget.h"

class QMenuBar;
class QAction;
class QPixmap;
class QPoint;
class QMenu;

class PsiCon;
class PsiToolBar;
class PsiAccount;
class IconAction;
class PsiIcon;
class ContactView;
namespace XMPP {
	class Status;
}

class MainWin : public AdvancedWidget<QMainWindow>
{
	Q_OBJECT
public:
	MainWin(bool onTop, bool asTool, PsiCon *, const char *name=0);
	~MainWin();

	void setWindowOpts(bool onTop, bool asTool);
	void setUseDock(bool);

	void buildToolbars();

	// evil stuff! remove ASAP!!
	QStringList actionList;
	QMap<QString, QAction*> actions;

	ContactView *cvlist;

	PsiCon *psiCon() const;

protected:
	// reimplemented
	void closeEvent(QCloseEvent *);
	void keyPressEvent(QKeyEvent *);
	QMenuBar* mainMenuBar() const;
#ifdef Q_WS_WIN
	bool winEvent(MSG *, long *);
#endif

signals:
	void statusChanged(int);
	void changeProfile();
	void blankMessage();
	void closeProgram();
	void doOptions();
	void doToolbars();
	void doManageAccounts();
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

	void setTrayToolTip(int);

	void activatedStatusAction(int);

	void trayClicked(const QPoint &, int);
	void trayDoubleClicked();
	void trayShow();
	void trayHide();

	void doRecvNextEvent();
	void statusClicked(int);
	void try2tryCloseProgram();
	void tryCloseProgram();

	void numAccountsChanged();
	void accountFeaturesChanged();
	void activatedAccOption(PsiAccount *, int);

	void actReadmeActivated ();
	void actOnlineHelpActivated ();
	void actOnlineWikiActivated ();
	void actOnlineHomeActivated ();
	void actOnlineForumActivated ();
	void actJoinPsiMUCActivated();
	void actBugReportActivated ();
	void actAboutActivated ();
	void actAboutQtActivated ();
	void actPlaySoundsActivated (bool);
	void actPublishTuneActivated (bool);
	void actTipActivated();
	void actDiagQCAPluginActivated();
	void actDiagQCAKeyStoreActivated();

	bool showDockMenu(const QPoint &);
	void dockActivated();
	
	void searchClearClicked();
	void searchTextEntered(QString const &text);
	void searchTextStarted(QString const &text);

	void registerAction( IconAction * );

public slots:
	void setWindowIcon(const QPixmap&);
	void showNoFocus();

	void decorateButton(int);
	void updateReadNext(PsiIcon *nextAnim, int nextAmount);

	void optionsUpdate();
	void setTrayToolTip(const XMPP::Status &, bool usePriority = false);

	void toggleVisible();

private:
	void buildGeneralMenu(QMenu *);
	QString numEventsString(int) const;

	bool askQuit();

	void updateCaption();
	void updateTray();

	void saveToolbarsState();
	void loadToolbarsState();

private:
	class Private;
	Private *d;
	friend class Private;
	QList<PsiToolBar*> toolbars_;
};

#endif
