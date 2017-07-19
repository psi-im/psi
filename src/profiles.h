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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef PROFILES_H
#define PROFILES_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QtCrypto>
#include <QByteArray>

#include "varlist.h"
#include "proxy.h"
#include "common.h"
#include "xmpp_clientstream.h"
#include "xmpp_roster.h"
#include "xmpp_jid.h"
#include "applicationinfo.h"

class OptionsTree;

class UserAccount
{
public:
	UserAccount();
	~UserAccount();

	void reset();

	void fromXml(const QDomElement &);

	void fromOptions(OptionsTree *o, QString base);
	void toOptions(OptionsTree *o, QString base=QString());
	int defaultPriority(const XMPP::Status &s);
	void saveLastStatus(OptionsTree *o, QString base);

	QString id;
	QString name;
	QString jid, pass, host, resource, authid, realm;
	bool customAuth;
	bool storeSaltedHashedPassword;
	QString scramSaltedHashPassword;
	int port, priority;
	bool opt_enabled, opt_pass, opt_host, opt_auto, opt_keepAlive, opt_log, opt_connectAfterSleep, opt_autoSameStatus, opt_reconn, opt_ignoreSSLWarnings, opt_compress, opt_sm;
	XMPP::ClientStream::AllowPlainType allow_plain;
	bool req_mutual_auth;
	bool legacy_ssl_probe;
	bool opt_automatic_resource, priority_dep_on_status, ignore_global_actions;
	int security_level;
	enum SSLFlag { SSL_No = 0, SSL_Yes = 1, SSL_Auto = 2, SSL_Legacy = 3 } ssl;

	QString proxyID;

	XMPP::Roster roster;
	XMPP::Status lastStatus;
	bool lastStatusWithPriority;

	struct GroupData {
		bool open;
		int  rank;
	};
	QMap<QString, GroupData> groupState;

	QCA::PGPKey pgpSecretKey;
	QString pgpPassPhrase;

	VarList keybind;

	XMPP::Jid dtProxy;
	bool ibbOnly;

	QStringList alwaysVisibleContacts;
	QStringList localMucBookmarks, ignoreMucBookmarks;

	QString optionsBase;

	QStringList stunHosts;
	QString stunHost;
	QString stunUser;
	QString stunPass;

	QByteArray tlsOverrideCert;
	QString tlsOverrideDomain;

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
	void lateMigration();

	//QString progver;
	UserAccountList accMigration;

	ProxyItemList proxyMigration;

private:
	lateMigrationOptions lateMigrationData;
};

QString pathToProfile(const QString &, ApplicationInfo::HomedirType type);
QString pathToProfileConfig(const QString &);
QStringList getProfilesList();
bool profileExists(const QString &);
bool profileNew(const QString &);
bool profileRename(const QString &, const QString &);
bool profileDelete(const QStringList &);

extern QString activeProfile;

#endif
