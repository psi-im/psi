/*
 * wbdlg.h - dialog for whiteboarding
 * Copyright (C) 2006  Joonas Govenius
 *
 * Originally developed from:
 * wbdlg.h - dialog for handling chats
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

#ifndef WBDLG_H
#define WBDLG_H

#include <QToolBar>
#include <QLabel>
#include <QLineEdit>
#include <QContextMenuEvent>
#include <QShowEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QInputDialog>
// #include <QMessageBoxEx>

#include "advwidget.h"
#include "im.h"
#include "common.h"
#include "wbwidget.h"
#include "iconlabel.h"
#include "psiaccount.h"
#include "psioptions.h"

class AccountLabel;

using namespace XMPP;

/*! \brief The dialog for a whiteboard session.
 *
 *  Contains the WbWidget and provides controls for setting the mode, stroke
 *  width and color of new items.
 *
 *  Also provides controls for setting general properties of the session and
 *  a control for ending and saving the session.
 * 
 *  \sa WbManager
 *  \sa WbWidget
 */
class WbDlg : public AdvancedWidget<QWidget>
{
	Q_OBJECT

public:
	/*! \brief Constructor.
	*  Creates a new dialog for the specified jid and session.
	*/
	WbDlg(SxeSession* session, PsiAccount* pa);
	/*! \brief Destructor.
	*  Emits sessionEnded()
	*/
	~WbDlg();

	/*! \brief Returns the session identifier.*/
	SxeSession* session() const;
	/*! \brief Returns whether further edits to the session are allowed.*/
		bool allowEdits() const;
	/*! \brief Sets whether further edits to the session are allowed.*/
	void setAllowEdits(bool);

public slots:
	/*! \brief Ends the session.
	 *  Asks for confirmation if invoked by user's action.
	 */
	void endSession();
	/*! \brief Removes indicators of new edits.*/
	void activated();
	/*! \brief Notifies the user that \a peer left the session.*/
	void peerLeftSession(const Jid &peer);

signals:
	/*! \brief Signals that the session ended and the dialog is to be deleted.*/
	void sessionEnded(WbDlg* dialog);

protected:
	// reimplemented
	/*! \brief Catches keyboard shortcuts.*/
	void keyPressEvent(QKeyEvent *);
	/*! \brief Sets the destruction times as specified by options.*/
	void closeEvent(QCloseEvent *);
	/*! \brief Saves the size of the dialog as default if so specified in options.*/
	void resizeEvent(QResizeEvent *);
	/*! \brief Removes the destruction timer.*/
	void showEvent(QShowEvent *);
	/*! \brief Invokes activated() if activated.*/
	void changeEvent(QEvent *e);

private slots:
	/*! \brief Popsup a dialog for saving the contents of the whiteboard to an SVG file. */
	void save();
	/*! \brief Popsup a dialog asking for a new viewBox for the whiteboard.*/
	void setGeometry();
	/*! \brief Popsup a color dialog and sets the selected color as the default stroke color for new items.*/
	void setStrokeColor();
	/*! \brief Popsup a color dialog and sets the selected color as the default fill color for new items.*/
	void setFillColor();
	/*! \brief Sets the stroke width for new elements based on the invoker.*/
	void setStrokeWidth(QAction *);
	/*! \brief Sets the WbWidget's mode based on the invoker.*/
	void setMode(QAction *);
	/*! \brief Sets keep open false.*/
	void setKeepOpenFalse();
	/*! \brief Constructs the context menu.*/
	void buildMenu();

private:
	/*! \brief Pops up the context menu.*/
	void contextMenuEvent(QContextMenuEvent *);
	/*! \brief Set a timer for self destruction in the give n number of minutes.*/
	void setSelfDestruct(int);
	/*! \brief Update the caption to indicate the number of unseen whiteboard messages.*/
	void updateCaption();

	/*! \brief The main widget.*/
	WbWidget *wbWidget_;

	/*! \brief The label showing own identity.*/
	AccountLabel *lb_ident_;
	/*! \brief The line edit showing target JID.*/
	QLineEdit *le_jid_;
	/*! \brief The context menu.*/
	QMenu *pm_settings_;

	/*! \brief The main toolbar.*/
	QToolBar *toolbar_;
	/*! \brief The action group for colors.*/
	QActionGroup *group_colors_;
	/*! \brief The action group for widths.*/
	QActionGroup *group_widths_;
	/*! \brief The action group for modes.*/
	QActionGroup *group_modes_;
	/*! \brief The menu for colors.*/
	QMenu *menu_colors_;
	/*! \brief The menu for widths.*/
	QMenu *menu_widths_;
	/*! \brief The menu for modes.*/
	QMenu *menu_modes_;
	QAction *act_color_, *act_fill_;
	IconAction *act_end_, *act_clear_, *act_save_, *act_geometry_, *act_widths_, *act_modes_;
	/*! \brief True if the target is a groupchat.*/
	bool groupChat_;
	/*! \brief The number of whiteboard messages since last activation.*/
	uint pending_;
	/*! \brief Boolean set true if a new edit was just received.*/
	bool keepOpen_;
	/*! \brief Boolean about whether further edits to the session are allowed.*/
	bool allowEdits_;

	/*! \brief Pointer to the timer that will invoke destruction.*/
	QTimer *selfDestruct_;
};

#endif
