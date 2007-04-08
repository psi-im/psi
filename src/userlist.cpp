/*
 * userlist.cpp - high-level roster
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

#include <QApplication>
#include <QPixmap>
#include <QList>
#include <QtCrypto>
#include <QTextDocument> // for Qt::escape()

#include "userlist.h"
#include "avatars.h"
#include "im.h"
#include "textutil.h"
#include "common.h"
#include "mucmanager.h"
#include "psioptions.h"
#include "jidutil.h"

using namespace XMPP;

static QString dot_truncate(const QString &in, int clip)
{
	if((int)in.length() <= clip)
		return in;
	QString s = in;
	s.truncate(clip);
	s += "...";
	return s;
}

//----------------------------------------------------------------------------
// UserResource
//----------------------------------------------------------------------------
UserResource::UserResource()
:Resource()
{
}

UserResource::UserResource(const Resource &r)
{
	setResource(r);
}

UserResource::~UserResource()
{
}

void UserResource::setResource(const Resource &r)
{
	setName(r.name());
	setStatus(r.status());
}

const QString & UserResource::versionString() const
{
	return v_ver;
}

const QString & UserResource::clientName() const
{
	return v_clientName;
}

const QString & UserResource::clientVersion() const
{
	return v_clientVersion;
}

const QString & UserResource::clientOS() const
{
	return v_clientOS;
}

void UserResource::setClient(const QString &name, const QString& version, const QString& os)
{
	v_clientName = name;
	v_clientVersion = version;
	v_clientOS = os;
	if (!v_clientName.isEmpty()) {
		v_ver = v_clientName + " " + v_clientVersion;
		if ( !v_clientOS.isEmpty() )
			v_ver += " / " + v_clientOS;
	}
	else {
		v_ver = "";
	}
}

const QString & UserResource::publicKeyID() const
{
	return v_keyID;
}

int UserResource::pgpVerifyStatus() const
{
	return v_pgpVerifyStatus;
}

QDateTime UserResource::sigTimestamp() const
{
	return sigts;
}

void UserResource::setPublicKeyID(const QString &s)
{
	v_keyID = s;
}

void UserResource::setPGPVerifyStatus(int x)
{
	v_pgpVerifyStatus = x;
}

void UserResource::setSigTimestamp(const QDateTime &ts)
{
	sigts = ts;
}

void UserResource::setTune(const QString& t)
{
	v_tune = t;
}

const QString& UserResource::tune() const
{
	return v_tune;
}

void UserResource::setGeoLocation(const GeoLocation& geoLocation)
{
	v_geoLocation = geoLocation;
}

const GeoLocation& UserResource::geoLocation() const
{
	return v_geoLocation;
}

void UserResource::setPhysicalLocation(const PhysicalLocation& physicalLocation)
{
	v_physicalLocation = physicalLocation;
}

const PhysicalLocation& UserResource::physicalLocation() const
{
	return v_physicalLocation;
}


bool operator<(const UserResource &r1, const UserResource &r2)
{
	return r1.priority() > r2.priority();
}

bool operator<=(const UserResource &r1, const UserResource &r2)
{
	return r1.priority() >= r2.priority();
}

bool operator==(const UserResource &r1, const UserResource &r2)
{
	return r1.priority() == r2.priority();
}

bool operator>(const UserResource &r1, const UserResource &r2)
{
	return r1.priority() < r2.priority();
}

bool operator>=(const UserResource &r1, const UserResource &r2)
{
	return r1.priority() <= r2.priority();
}


//----------------------------------------------------------------------------
// UserResourceList
//----------------------------------------------------------------------------
UserResourceList::UserResourceList()
:QList<UserResource>()
{
}

UserResourceList::~UserResourceList()
{
}

UserResourceList::Iterator UserResourceList::find(const QString & _find)
{
	for(UserResourceList::Iterator it = begin(); it != end(); ++it) {
		if((*it).name() == _find)
			return it;
	}

	return end();
}

UserResourceList::Iterator UserResourceList::priority()
{
	UserResourceList::Iterator highest = end();

	for(UserResourceList::Iterator it = begin(); it != end(); ++it) {
		if(highest == end() || (*it).priority() > (*highest).priority())
			highest = it;
	}

	return highest;
}

UserResourceList::ConstIterator UserResourceList::find(const QString & _find) const
{
	for(UserResourceList::ConstIterator it = begin(); it != end(); ++it) {
		if((*it).name() == _find)
			return it;
	}

	return end();
}

UserResourceList::ConstIterator UserResourceList::priority() const
{
	UserResourceList::ConstIterator highest = end();

	for(UserResourceList::ConstIterator it = begin(); it != end(); ++it) {
		if(highest == end() || (*it).priority() > (*highest).priority())
			highest = it;
	}

	return highest;
}

void UserResourceList::sort()
{
	qSort(*this);
}


//----------------------------------------------------------------------------
// UserListItem
//----------------------------------------------------------------------------
UserListItem::UserListItem(bool self)
{
	v_inList = false;
	v_self = self;
	v_private = false;
	v_avatarFactory = NULL;
	lastmsgtype = -1;
}

UserListItem::~UserListItem()
{
}

bool UserListItem::inList() const
{
	return v_inList;
}

void UserListItem::setMood(const Mood& mood)
{
	v_mood = mood;
}

const Mood& UserListItem::mood() const
{
	return v_mood;
}

void UserListItem::setTune(const QString& t)
{
	v_tune = t;
}

const QString& UserListItem::tune() const
{
	return v_tune;
}

void UserListItem::setGeoLocation(const GeoLocation& geoLocation)
{
	v_geoLocation = geoLocation;
}

const GeoLocation& UserListItem::geoLocation() const
{
	return v_geoLocation;
}

void UserListItem::setPhysicalLocation(const PhysicalLocation& physicalLocation)
{
	v_physicalLocation = physicalLocation;
}

const PhysicalLocation& UserListItem::physicalLocation() const
{
	return v_physicalLocation;
}

void UserListItem::setAvatarFactory(AvatarFactory* av)
{
	v_avatarFactory = av;
}

void UserListItem::setJid(const Jid &j)
{
	LiveRosterItem::setJid(j);

	int n = jid().full().find('@');
	if(n == -1)
		v_isTransport = true;
	else
		v_isTransport = false;
}

bool UserListItem::isTransport() const
{
	return v_isTransport;
}

bool UserListItem::isAvailable() const
{
	return !v_url.isEmpty();
}

bool UserListItem::isHidden() const
{
	return groups().contains(qApp->translate("ContactView", "Hidden"));
}

bool UserListItem::isAway() const
{
	int status;
	if(!isAvailable())
		status = STATUS_OFFLINE;
	else
		status = makeSTATUS((*userResourceList().priority()).status());

	if(status == STATUS_AWAY || status == STATUS_XA || status == STATUS_DND)
		return true;
	else
		return false;
}

QDateTime UserListItem::lastAvailable() const
{
	return v_t;
}

int UserListItem::lastMessageType() const
{
	return lastmsgtype;
}

void UserListItem::setLastMessageType(const int mtype)
{
//	printf("setting message type to %i\n", mtype);
	lastmsgtype = mtype;
}

const QString & UserListItem::presenceError() const
{
	return v_perr;
}

bool UserListItem::isSelf() const
{
	return v_self;
}

void UserListItem::setInList(bool b)
{
	v_inList = b;
}

void UserListItem::setLastAvailable(const QDateTime &t)
{
	v_t = t;
}

void UserListItem::setPresenceError(const QString &e)
{
	v_perr = e;
}

UserResourceList & UserListItem::userResourceList()
{
	return v_url;
}

UserResourceList::Iterator UserListItem::priority()
{
	return v_url.priority();
}

const UserResourceList & UserListItem::userResourceList() const
{
	return v_url;
}

UserResourceList::ConstIterator UserListItem::priority() const
{
	return v_url.priority();
}

QString UserListItem::makeTip(bool trim, bool doLinkify) const
{
	return "<qt>" + makeBareTip(trim,doLinkify) + "</qt>";
}

QString UserListItem::makeBareTip(bool trim, bool doLinkify) const
{
	QString str;
	bool useAvatar = false;
	if (v_avatarFactory && !v_avatarFactory->getAvatar(jid().bare()).isNull() && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.avatar").toBool())
		useAvatar = true;

	if (useAvatar) {
		str += "<table cellspacing=\"0\"><tr>";
		str += "<td>";
		str += QString("<icon name=\"avatars/%1\">").arg(jid().bare());
		str += "</td><td width=\"10\"></td>";
		str += "<td>";
	}

	QString nick = JIDUtil::nickOrJid(name(), jid().full());
	if(jid().full() != nick)
		str += QString("<div style='white-space:pre'>%1 &lt;%2&gt;</div>").arg(Qt::escape(nick)).arg(Qt::escape(JIDUtil::toString(jid(),true)));
	else
		str += QString("<div style='white-space:pre'>%1</div>").arg(Qt::escape(nick));

	// subscription
	if(!v_self && subscription().type() != Subscription::Both)
		str += QString("<div style='white-space:pre'>") + QObject::tr("Subscription") + ": " + subscription().toString() + "</div>";

	if(!v_keyID.isEmpty())
		str += QString("<div style='white-space:pre'>") + QObject::tr("OpenPGP") + ": " + v_keyID.right(8) + "</div>";

	// User Mood
	if (!mood().isNull()) {
		str += QString("<div style='white-space:pre'>") + QObject::tr("Mood") + ": " + mood().typeText();
		if (!mood().text().isEmpty())
			str += QString(" (") + mood().text() + QString(")");
		str += "</div>";
	}
	
	// User Tune
	if (!tune().isEmpty())
		str += QString("<div style='white-space:pre'>") + QObject::tr("Listening to") + ": " + tune() + "</div>";

	// User Physical Location
	if (!physicalLocation().isNull())
		str += QString("<div style='white-space:pre'>") + QObject::tr("Location") + ": " + physicalLocation().toString() + "</div>";

	// User Geolocation
	if (!geoLocation().isNull())
		str += QString("<div style='white-space:pre'>") + QObject::tr("Geolocation") + ": " + QString::number(geoLocation().lat().value()) + "/" + QString::number(geoLocation().lon().value()) + "</div>";

	// resources
	if(!userResourceList().isEmpty()) {
		UserResourceList srl = userResourceList();
		srl.sort();

		for(UserResourceList::ConstIterator rit = srl.begin(); rit != srl.end(); ++rit) {
			const UserResource &r = *rit;
			QString name;
			if(!r.name().isEmpty())
				name = r.name();
			else
				name = QObject::tr("[blank]");

			int status = makeSTATUS(r.status());
			QString istr = "status/offline";
			if(status == STATUS_ONLINE)
				istr = "status/online";
			else if(status == STATUS_AWAY)
				istr = "status/away";
			else if(status == STATUS_XA)
				istr = "status/xa";
			else if(status == STATUS_DND)
				istr = "status/dnd";
			else if(status == STATUS_CHAT)
				istr = "status/chat";
			else if(status == STATUS_INVISIBLE)
				istr = "status/invisible"; //this shouldn't happen

			QString imgTag = "icon name"; // or 'img src' if appropriate QMimeSourceFactory is installed. but mblsha noticed that QMimeSourceFactory unloads sometimes
			QString secstr;
			if(isSecure(r.name()) && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.pgp").toBool())
				secstr += QString(" <%1=\"psi/cryptoYes\">").arg(imgTag);
			str += QString("<div style='white-space:pre'>") + QString("<%1=\"%2\"> ").arg(imgTag).arg(istr) + QString("<b>%1</b> ").arg(Qt::escape(name)) + QString("(%1)").arg(r.priority()) + secstr + "</div>";

			if(!r.publicKeyID().isEmpty() && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.pgp").toBool()) {
				int v = r.pgpVerifyStatus();
				if(v == QCA::SecureMessageSignature::Valid || v == QCA::SecureMessageSignature::InvalidSignature || v == QCA::SecureMessageSignature::InvalidKey || v == QCA::SecureMessageSignature::NoKey) {
					if(v == QCA::SecureMessageSignature::Valid) {
						QString d = r.sigTimestamp().toString(Qt::TextDate);
						str += QString("<div style='white-space:pre'>") + QObject::tr("Signed") + " @ " + "<font color=\"#2A993B\">" + d + "</font>";
					}
					else if(v == QCA::SecureMessageSignature::NoKey) {
						QString d = r.sigTimestamp().toString(Qt::TextDate);
						str += QString("<div style='white-space:pre'>") + QObject::tr("Signed") + " @ " + d;
					}
					else if(v == QCA::SecureMessageSignature::InvalidSignature || v == QCA::SecureMessageSignature::InvalidKey) {
						str += QString("<div style='white-space:pre'>") + "<font color=\"#810000\">" + QObject::tr("Bad signature") + "</font>";
					}

					if(v_keyID != r.publicKeyID())
						str += QString(" [%1]").arg(r.publicKeyID().right(8));
					str += "</div>";
				}
			}

			// last status
			if(r.status().timeStamp().isValid() && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.last-status").toBool()) {
				QString d = r.status().timeStamp().toString(Qt::TextDate);
				str += QString("<div style='white-space:pre'>") + QObject::tr("Last Status") + " @ " + d + "</div>";
			}

			// MUC
			if(r.status().hasMUCItem()) {
				if(!r.status().mucItem().jid().isEmpty())
					str += QString("<div style='white-space:pre'>") + QObject::tr("JID: %1").arg(JIDUtil::toString(r.status().mucItem().jid(),true)) + QString("</div>");
				if(r.status().mucItem().role() != MUCItem::NoRole)
					str += QString("<div style='white-space:pre'>") + QObject::tr("Role: %1").arg(MUCManager::roleToString(r.status().mucItem().role())) + QString("</div>");
				str += QString("<div style='white-space:pre'>") + QObject::tr("Affiliation: %1").arg(MUCManager::affiliationToString(r.status().mucItem().affiliation())) + QString("</div>");
			}

			// gabber music
			if(!r.status().songTitle().isEmpty()) {
				QString s = r.status().songTitle();
				if(trim)
					s = dot_truncate(s, 80);
				s = Qt::escape(s);
				str += QString("<div style='white-space:pre'>") + QObject::tr("Listening to") + QString(": %1").arg(s) + "</div>";
			}
			
			// User Tune
			if (!r.tune().isEmpty())
				str += QString("<div style='white-space:pre'>") + QObject::tr("Listening to") + ": " + r.tune() + "</div>";

			// User Physical Location
			if (!r.physicalLocation().isNull())
				str += QString("<div style='white-space:pre'>") + QObject::tr("Location") + ": " + r.physicalLocation().toString() + "</div>";

			// User Geolocation
			if (!r.geoLocation().isNull())
				str += QString("<div style='white-space:pre'>") + QObject::tr("Geolocation") + ": " + QString::number(r.geoLocation().lat().value()) + "/" + QString::number(r.geoLocation().lon().value()) + "</div>";

			// client
			if(!r.versionString().isEmpty() && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.client-version").toBool()) {
				QString ver = r.versionString();
				if(trim)
					ver = dot_truncate(ver, 80);
				ver = Qt::escape(ver);
				str += QString("<div style='white-space:pre'>") + QObject::tr("Using") + QString(": %1").arg(ver) + "</div>";
			}

			// status message
			QString s = r.status().status();
			if(!s.isEmpty()) {
				QString head = QObject::tr("Status Message");
				if(trim)
					s = TextUtil::plain2rich(clipStatus(s, 200, 12));
				else
					s = TextUtil::plain2rich(s);
				if ( doLinkify )
					s = TextUtil::linkify(s);
				if( option.useEmoticons && !doLinkify )
					s = TextUtil::emoticonify(s);
				if( !doLinkify && PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool() )
					s = TextUtil::legacyFormat(s);
				str += QString("<div style='white-space:pre'><u>%1</u></div><div>%2</div>").arg(head).arg(s);
			}
		}
	}
	else {
		// last available
		if(!lastAvailable().isNull()) {
			QString d = lastAvailable().toString(Qt::TextDate);
			str += QString("<div style='white-space:pre'>") + QObject::tr("Last Available") + " @ " + d + "</div>";
		}

		// presence error
		if(!v_perr.isEmpty()) {
			QStringList err = v_perr.split('\n');
			str += QString("<div style='white-space:pre'>") + QObject::tr("Presence Error") + QString(": %1").arg(Qt::escape(err[0])) + "</div>";
			err.pop_front();
			foreach (QString line, err)
				str += "<div>" + line + "</div>";
		}

		// status message
		QString s = lastUnavailableStatus().status();
		if(!s.isEmpty()) {
			QString head = QObject::tr("Last Status Message");
			if(trim)
				s = TextUtil::plain2rich(clipStatus(s, 200, 12));
			else {
				s = TextUtil::plain2rich(clipStatus(s, 200, 12));
				if ( doLinkify )
					s = TextUtil::linkify(s);
			}
			str += QString("<div style='white-space:pre'><u>%1</u></div><div>%2</div>").arg(head).arg(s);
		}
	}

	if (useAvatar) {
		str += "</td>";
		str += "</tr></table>";
	}

	return str;
}

QString UserListItem::makeDesc() const
{
	return makeTip(false);
}

bool UserListItem::isPrivate() const
{
	return v_private;
}

void UserListItem::setPrivate(bool b)
{
	v_private = b;
}

bool UserListItem::isSecure(const QString &rname) const
{
	for(QStringList::ConstIterator it = secList.begin(); it != secList.end(); ++it) {
		if(*it == rname)
			return true;
	}
	return false;
}

void UserListItem::setSecure(const QString &rname, bool b)
{
	for(QStringList::Iterator it = secList.begin(); it != secList.end(); ++it) {
		if(*it == rname) {
			if(!b)
				secList.remove(it);
			return;
		}
	}
	if(b)
		secList.append(rname);
}

const QString & UserListItem::publicKeyID() const
{
	return v_keyID;
}

void UserListItem::setPublicKeyID(const QString &k)
{
	v_keyID = k;
}


//----------------------------------------------------------------------------
// UserList
//----------------------------------------------------------------------------
UserList::UserList()
{
}

UserList::~UserList()
{
}

UserListItem *UserList::find(const XMPP::Jid &j)
{
	UserListIt it(*this);
	for(UserListItem *i; (i = it.current()); ++it) {
		if(i->jid().compare(j))
			return i;
	}
	return 0;
}

