/*
 * mucmanager.h
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef MUCMANAGER_H
#define MUCMANAGER_H

#include <QObject>

#include "xmpp_muc.h"
#include "xmpp_jid.h"

class QString;
namespace XMPP {
	class XData;
	class Client;
}

class PsiAccount;

using namespace XMPP;

class MUCManager : public QObject
{
	Q_OBJECT

public:
	enum Action {
		Unknown, Kick, Ban,
		GrantVoice, RevokeVoice,
		GrantMember, RevokeMember,
		GrantModerator, RevokeModerator,
		GrantAdmin, RevokeAdmin,
		GrantOwner, RevokeOwner,
		SetRole, SetAffiliation
	};

	MUCManager(PsiAccount* account, const Jid&);

	const Jid& room() const;

	void getConfiguration();
	void setConfiguration(const XMPP::XData&);
	void setDefaultConfiguration();
	void setItems(const QList<MUCItem>&);
	void getItemsByAffiliation(MUCItem::Affiliation);
	void destroy(const QString& reason = QString(), const Jid& venue = Jid());
	XMPP::Client* client() const;
	PsiAccount* account() const;

	// Basic operations
	void kick(const QString&, const QString& = QString());
	void grantVoice(const QString&, const QString& = QString());
	void revokeVoice(const QString&, const QString& = QString());
	void grantModerator(const QString&, const QString& = QString());
	void revokeModerator(const QString&, const QString& = QString());
	void ban(const Jid&, const QString& = QString());
	void grantMember(const Jid&, const QString& = QString());
	void revokeMember(const Jid&, const QString& = QString());
	void grantOwner(const Jid&, const QString& = QString());
	void revokeOwner(const Jid&, const QString& = QString());
	void grantAdmin(const Jid&, const QString& = QString());
	void revokeAdmin(const Jid&, const QString& = QString());
	void setRole(const QString&, MUCItem::Role, const QString& = QString(), Action = Unknown);
	void setAffiliation(const Jid&, MUCItem::Affiliation, const QString& = QString(), Action = Unknown);

	// Tests
	static QString roleToString(XMPP::MUCItem::Role, bool p = false);
	static QString affiliationToString(XMPP::MUCItem::Affiliation, bool p = false);
	static bool canKick(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canGrantVoice(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canRevokeVoice(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canGrantModerator(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canRevokeModerator(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canBan(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canGrantMember(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canRevokeMember(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canGrantOwner(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canRevokeOwner(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canGrantAdmin(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canRevokeAdmin(const XMPP::MUCItem&, const XMPP::MUCItem&);
	static bool canSetRole(const XMPP::MUCItem&, const XMPP::MUCItem&, XMPP::MUCItem::Role);
	static bool canSetAffiliation(const XMPP::MUCItem&, const XMPP::MUCItem&, XMPP::MUCItem::Affiliation);

signals:
	void getConfiguration_success(const XData&);
	void getConfiguration_error(int, const QString&);
	void setConfiguration_success();
	void setConfiguration_error(int, const QString&);
	void destroy_success();
	void destroy_error(int, const QString&);
	void setItems_success();
	void setItems_error(int, const QString&);
	void getItemsByAffiliation_success(MUCItem::Affiliation, const QList<MUCItem>&);
	void getItemsByAffiliation_error(MUCItem::Affiliation, int, const QString&);

	void action_success(MUCManager::Action);
	void action_error(MUCManager::Action, int, const QString&);


protected slots:
	void getConfiguration_finished();
	void setConfiguration_finished();
	void destroy_finished();
	void action_finished();
	void getItemsByAffiliation_finished();
	void setItems_finished();

private:
	PsiAccount* account_;
	Jid room_;
};

#endif
