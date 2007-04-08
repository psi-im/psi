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

#include <q3mainwindow.h>
#include <qmap.h>
//Added by qt3to4:
#include <Q3ActionGroup>
#include <QPixmap>
#include <QCloseEvent>
#include <QList>
#include <QKeyEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QMenu>
#include "im.h"
#include "contactview.h"
#include "psitoolbar.h"
#include "advwidget.h"

class PsiCon;
class PsiAccount;
class QIcon;
class IconLabel;

class QAction;
class Q3ActionGroup;
class IconAction;
class QLabel;
class QVBoxLayout;
class QMenuBar;

class MainWin : public AdvancedWidget<Q3MainWindow>
//class MainWin : public Q3MainWindow
{
	Q_OBJECT
public:
	MainWin(bool onTop, bool asTool, PsiCon *, const char *name=0);
	~MainWin();

	void setWindowOpts(bool onTop, bool asTool);
	void setUseDock(bool);
	void setInfo(const QString &);

	// evil stuff! remove ASAP!!
	QStringList actionList;
	QMap<QString, QAction*> actions;

	ContactView *cvlist;
	QList<PsiToolBar*> toolbars;

	void buildToolbars();
	void saveToolbarsPositions();
	PsiCon *psiCon() const;

protected:
	void closeEvent(QCloseEvent *);
	void keyPressEvent(QKeyEvent *);

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

	void geomChanged(int x, int y, int w, int h);

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
	void activatedAccOption(PsiAccount *, int);

	void actReadmeActivated ();
	void actOnlineHelpActivated ();
	void actOnlineWikiActivated ();
	void actOnlineHomeActivated ();
	void actBugReportActivated ();
	void actAboutActivated ();
	void actAboutQtActivated ();
	void actPlaySoundsActivated (bool);
	void actPublishTuneActivated (bool);
	void actTipActivated();

	bool showDockMenu(const QPoint &);
	void dockActivated();

	void registerAction( IconAction * );

public slots:
	void setWindowIcon(const QPixmap&);
	void showNoFocus();

	void decorateButton(int);
	void updateReadNext(Icon *nextAnim, int nextAmount);

	void optionsUpdate();
	void setTrayToolTip(const Status &, bool usePriority = false);

	void toggleVisible();

private:
	void buildGeneralMenu(QMenu *);
	QString numEventsString(int) const;

	bool askQuit();

	void updateCaption();
	void updateTray();

private:
	class Private;
	Private *d;
	friend class Private;
};

#endif
