/*
 * wbmanager.h - Whiteboard manager
 * Copyright (C) 2007  Joonas Govenius
 *
 * Influenced by:
 * pepmanager.h - Classes for PEP
 * Copyright (C) 2006  Remko Troncon
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

#ifndef WBMANAGER_H
#define WBMANAGER_H

#include "../sxe/sxemanager.h"
#include "wbdlg.h"

#include <QInputDialog>

namespace XMPP {
	class Client;
	class Jid;
	class Message;
}

using namespace XMPP;

/*! \brief The manager for whiteboard dialogs.
 *  The manager listen to SxeManager to pick up any new sessions negotiated by
 *  it and creates a dialog for each session.
 *
 *  The manager also provides a possibillity for the local client to start
 *  a new session negotiation with the desired contact.
 *
 *  \sa WbDlg
 */
class WbManager : public QObject
{
	Q_OBJECT

public:
	/*! \brief Constructor.
	 *  Creates a new manager for the specified Client and PsiAccount
	 */
	WbManager(XMPP::Client* client, PsiAccount* pa, SxeManager* sxemanager);
	/*! \brief Returns true if features contains WBNS and the user wishes to accept the invitation. */
    static bool checkInvitation(const Jid &peer, const QList<QString> &features);

public slots:
	/*! \brief Opens the existing dialog to the specified contact or starts a new session negotiation if necessary.*/
	void openWhiteboard(const Jid &target, const Jid &ownJid, bool groupChat, bool promptInitialDoc = true);

private:
	/*! \brief Return a pointer to a dialog to the specified contact.
	 *  If such dialog doesn't exits, returns 0.
	 */
	WbDlg* findWbDlg(const Jid &target);

	/*! \brief A pointer to the PsiAccount to pass to new dialogs.*/
	PsiAccount* pa_;
	/*! \brief A list of dialogs of established sessions.*/
	QList<WbDlg*> dialogs_;
	/*! \brief A pointer to the SxeManager used for negotiating sessions.*/
    SxeManager* sxemanager_;

private slots:
	/*! \brief Removes and deletes the dialog for the given session.*/
	void removeDialog(WbDlg* dialog);
	/*! \brief Returns a pointer to a new dialog to with given contact and session set.*/
	void createWbDlg(SxeSession* session);
};

#endif
