/*
 * wbmanager.cpp - Whiteboard manager
 * Copyright (C) 2007  Joonas Govenius
 *
 * Influenced by:
 * pepmanager.cpp - Classes for PEP
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
 
#include "wbmanager.h"
#include "psipopup.h"
#include <QDebug>
#include <QMessageBox>

#define WBNS "http://www.w3.org/2000/svg"
#define EMPTYWB "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\"> <svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.2\" viewBox=\"0 0 600 400\" baseProfile=\"tiny\" />"

using namespace XMPP;

// -----------------------------------------------------------------------------

WbManager::WbManager(XMPP::Client* client, PsiAccount* pa, SxeManager* sxemanager) {
	pa_ = pa;
	sxemanager_ = sxemanager;

	client->addExtension("whiteboard", Features(WBNS));

	connect(sxemanager_, SIGNAL(sessionNegotiated(SxeSession*)), SLOT(createWbDlg(SxeSession*)));
	sxemanager_->addInvitationCallback(WbManager::checkInvitation);
}

void WbManager::openWhiteboard(const Jid &target, const Jid &ownJid, bool groupChat, bool promptInitialDoc) {

	// check that the target supports whiteboarding via SXE
	QList<QString> features;
	features += WBNS;
	if(!sxemanager_->checkSupport(target, features)) {
		QMessageBox::information(NULL, tr("Unsupported"), tr("The contact does not support whiteboarding."));
		return;
	}


	// See if we have a session for the JID
	WbDlg* w = findWbDlg(target);
	if(!w) {
		// else negotiate a new session and return null

		QDomDocument doc;

		if(promptInitialDoc) {
			bool openExisting = (QMessageBox::Yes == QMessageBox::question(NULL,
														tr("Open Existing SVG?"),
														tr("Would you like to open an existing SVG document in the whitebaord?"),
														QMessageBox::Yes | QMessageBox::No,
														QMessageBox::No));

			if(openExisting) {
				// prompt for an existing file
				QString fileName = QFileDialog::getOpenFileName(NULL, tr("Initial SVG Document for the Whiteboard"),
																 QDir::homePath(),
																 tr("Scalable Vector Graphics (*.svg)"));

				QFile file(fileName);
				if(file.open(QIODevice::ReadOnly)) {
					doc.setContent(file.readAll(), true);
					file.close();
				}
			}
		}

		if(doc.documentElement().nodeName() != "svg") {

			// initialize with an empty whiteboarding document
			doc = QDomDocument();
			doc.setContent(QString(EMPTYWB), true);

		}


		// negotiate the session
		sxemanager_->startNewSession(target, ownJid, groupChat, doc, features);
	}
	else
		bringToFront(w);
}

void WbManager::removeDialog(WbDlg* dialog)
{
	dialogs_.takeAt(dialogs_.indexOf(dialog))->deleteLater();
}

WbDlg* WbManager::findWbDlg(const Jid &jid) {
	// find if a dialog for the jid already exists
	foreach(WbDlg* w, dialogs_) {
		// does the jid match?
		if(w->session()->target().compare(jid)) {
			return w;
		}
	}
	return 0;
}

void WbManager::createWbDlg(SxeSession* session) {
	// check if the session is a whiteboarding session
	bool whiteboarding = false;
	foreach(QString feature, session->features()) {
		if(feature == WBNS)
			whiteboarding = true;
	}

	if(whiteboarding) {
		// create the WbDlg
		WbDlg* w = new WbDlg(session, pa_);

		// connect the signals
		connect(w, SIGNAL(sessionEnded(WbDlg*)), SLOT(removeDialog(WbDlg*)));
		connect(session, SIGNAL(peerLeftSession(Jid)), w, SLOT(peerLeftSession(Jid)));

		dialogs_.append(w);

		bringToFront(w);
	}
}

bool WbManager::checkInvitation(const Jid &peer, const QList<QString> &features) {
	if(!features.contains(WBNS))
		return false;

	return (QMessageBox::Yes == QMessageBox::question(NULL,
									tr("Whiteboarding Invitation?"),
									tr("%1 has invited you to a whiteboarding session. Would you like to join?").arg(peer.full()),
									QMessageBox::Yes | QMessageBox::No,
									QMessageBox::No));
}
