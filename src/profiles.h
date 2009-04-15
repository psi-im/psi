/*
 * profiles.h - deal with profiles
 * Copyright (C) 2001-2003  Justin Karneges
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

#ifndef PROFILES_H
#define PROFILES_H

#include <qstring.h>
#include <qstringlist.h>
#include <QList>
#include <QtCrypto>

#include "varlist.h"
#include "proxy.h"
#include "common.h"
#include "xmpp_clientstream.h"
#include "xmpp_roster.h"
#include "xmpp_jid.h"

class OptionsTree;

class UserAccount
{
public:
	UserAccount();
	~UserAccount();

	void reset();

	QDomElement toXml(QDomDocument &, const QString &tagName);
	void fromXml(const QDomElement &);
	
	void fromOptions(OptionsTree *o, QString base);
	void toOptions(OptionsTree *o, QString base=QString());

	QString name;
	QString jid, pass, host, resource, authid, realm;
	bool customAuth;
	int port, priority;
 	bool opt_enabled, opt_pass, opt_host, opt_auto, opt_keepAlive, opt_log, opt_connectAfterSleep, opt_reconn, opt_ignoreSSLWarnings, opt_compress;
	XMPP::ClientStream::AllowPlainType allow_plain;
	bool req_mutual_auth;
	bool legacy_ssl_probe;
	bool opt_automatic_resource;
	int security_level;
	enum SSLFlag { SSL_No = 0, SSL_Yes = 1, SSL_Auto = 2, SSL_Legacy = 3 } ssl;

	QString proxyID;

	XMPP::Roster roster;

	struct GroupData {
		bool open;
		int  rank;
	};
	QMap<QString, GroupData> groupState;

	QCA::PGPKey pgpSecretKey;

	VarList keybind;

	XMPP::Jid dtProxy;
	
	QString optionsBase;

	QString stunHost;
	int stunPort;

	/* migration only */
	int proxy_index;
	int proxy_type, proxy_port;
	QString proxy_host, proxy_user, proxy_pass;
	bool tog_offline, tog_away, tog_agents, tog_hidden, tog_self;
};

typedef QList<UserAccount> UserAccountList;


class OptionsMigration
{
public:
	bool fromFile(const QString &);
	
	void lateMigration();

	//QString progver;
	UserAccountList accMigration;

	ProxyItemList proxyMigration;
	
private:
	lateMigrationOptions lateMigrationData;
};

QString pathToProfile(const QString &);
QString pathToProfileConfig(const QString &);
QStringList getProfilesList();
bool profileExists(const QString &);
bool profileNew(const QString &);
bool profileRename(const QString &, const QString &);
bool profileDelete(const QString &);

extern QString activeProfile;

#endif
