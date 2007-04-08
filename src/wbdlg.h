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
 *  Inherits Advanced Widget.
 *
 *  Takes in whiteboard elements for the session from the WbManager and passes
 *  them on to the WbWidget.
 *
 *  Contains the WbWidget and provides controls for setting the mode, stroke
 *  width and color of new items. TODO: Also provides controls for setting general
 *  properties of the session and a control for ending the session.
 * 
 *  \sa WbManager
 *  \sa WbWidget
 */
class WbDlg : public AdvancedWidget<QWidget>
{
	Q_OBJECT

	struct incomingElement {
		QDomElement wb;
		Jid sender;
	};

public:
	/*! \brief Constructor.
	*  Creates a new dialog for the specified jid and session.
	*/
	WbDlg(const Jid &target, const QString &session, const Jid &ownJid, bool groupChat, PsiAccount *pa);
	/*! \brief Destructor.
	*  Emits sessionEnded()
	*/
	~WbDlg();

	/*! \brief Passes the incoming element to the WbWidget and remembers the last edit.*/
	void incomingWbElement(const QDomElement &, const Jid &sender);
	/*! \brief Returns true if the target is a groupchat.*/
	const bool groupChat() const;
	/*! \brief Returns the target contact's JID.*/
	const Jid target() const;
	/*! \brief Returns the session identifier.*/
	const QString session() const;
	/*! \brief Returns the JID used by the user in the session.*/
	const Jid ownJid() const;
	/*! \brief Returns whether further edits to the session are allowed.*/
	bool allowEdits() const;
	/*! \brief Sets whether further edits to the session are allowed.*/
	void setAllowEdits(bool);
	/*! \brief Asks whether dialog should be deleted if peer left the session.*/
	void peerLeftSession();
	/*! \brief Sets whether new wb elements should be queued.*/
	void setQueueing(bool);
	/*! \brief Erases the wb elements from the incoming queue up to and including the wb identified by the parameters.
	 *  The parameters correspond to the attributes of the last-edit element.
	 */
	void eraseQueueUntil(QString sender, QString hash);
	/*! \brief Return the snapshot that was created when queueing was set true.*/
	QList<WbItem*> snapshot() const;
	/*! \brief Sets whether configure edits are accepted regardless of version.
	 *  Default is false. Should be set true if the session state is Negotiation::DocumentBegun.
	 *  Clears the whiteboard without emitting emitting wb elements when set true.
	 */
	void setImporting(bool);
	/*! \brief Return a hash with information identifying the last processed wb element.*/
	QHash<QString, QString> lastWb() const;
	/*! \brief Sets the information identifying the last processed wb.*/
	void setLastWb(const QString &sender, const QString &hash);

public slots:
	/*! \brief Ends the session.
	 *  Asks for confirmation if invoked by user's action.
	 */
	void endSession();
	/*! \brief Removes indicators of new edits.*/
	void activated();

signals:
	/*! \brief Passes the new whiteboard elements from the widget.*/
	void newWbElement(const QDomElement &, const Jid &, bool groupChat);
	/*! \brief Signals that the session ended and the dialog is to be deleted.*/
	void sessionEnded(const QString &);

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
	/*! \brief Emits the newWbElement() signal with the given element (usually from WbWidget).*/
	void doSend(const QDomElement &);
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

	/*! \brief The target JID.*/
	Jid target_;

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
	IconAction *act_end_, *act_clear_, *act_geometry_, *act_widths_, *act_modes_;
	/*! \brief List of queued incoming wb elements.*/
	QList<incomingElement> queuedIncomingElements_;
	/*! \brief List of queued outgoing wb elements.*/
	QList<QDomElement> queuedOutgoingElements_;
	/*! \brief An SVG document representing the the whiteboard when queued was set true.*/
	QList<WbItem*> snapshot_;
	/*! \brief True if the target is a groupchat.*/
	bool groupChat_;
	/*! \brief The number of whiteboard messages since last activation.*/
	uint pending_;
	/*! \brief Boolean set true if a new edit was just received.*/
	bool keepOpen_;
	/*! \brief Boolean about whether further edits to the session are allowed.*/
	bool allowEdits_;
	/*! \brief If true, new wb elements are queued rather than processed.*/
	bool queueing_;
	/*! \brief A string that identifies the last edit that was processed.*/
	QHash<QString, QString> lastWb_;

	/*! \brief Pointer to the timer that will invoke destruction.*/
	QTimer *selfDestruct_;
};

#endif
