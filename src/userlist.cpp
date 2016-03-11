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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QApplication>
#include <QPixmap>
#include <QList>
#include <QtCrypto>
#include <QTextDocument> // for TextUtil::escape()
#include <QBuffer>
#include <QUrl>

#include "userlist.h"
#include "avatars.h"
#include "textutil.h"
#include "common.h"
#include "mucmanager.h"
#include "psioptions.h"
#include "jidutil.h"
#include "psiiconset.h"
#include "common.h"

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

/**
 * \brief Timezone offset in minutes (if available).
 */
Maybe<int> UserResource::timezoneOffset() const
{
	return v_tzo;
}

/**
 * \brief Timezone offset as string (or empty string if no data).
 *
 * String is formatted as "UTC[+|-]h[:mm]".
 */
const QString& UserResource::timezoneOffsetString() const
{
	return v_tzoString;
}

/**
 * \brief Set timezone offset (in minutes).
 */
void UserResource::setTimezone(Maybe<int> off)
{
	v_tzo = off;

	if (off.hasValue()) {
		QTime t = QTime(0, 0).addSecs(abs(off.value())*60);
		QString u = QString("UTC") + (off.value() < 0 ? "-" : "+");
		u += QString::number(t.hour());
		if (t.minute())
			u += QString(":%1").arg(t.minute());
		v_tzoString = u;
	}
	else
		v_tzoString = "";
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

/*void UserResource::setPhysicalLocation(const PhysicalLocation& physicalLocation)
{
	v_physicalLocation = physicalLocation;
}

const PhysicalLocation& UserResource::physicalLocation() const
{
	return v_physicalLocation;
}*/


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
	v_isConference = false;
	v_avatarFactory = NULL;
	lastmsgtype = -1;
	v_pending = 0;
	v_hPending = 0;
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

QStringList UserListItem::clients() const
{
	QStringList res;

	//if(isMuc()) return res; //temporary commented out until necessary patches will be fixed
	if(!userResourceList().isEmpty()) {
		UserResourceList srl = userResourceList();
		srl.sort();

		for(UserResourceList::ConstIterator rit = srl.begin(); rit != srl.end(); ++rit) {
			res += findClient(*rit);
		}
	}
	return res;
}

QString UserListItem::findClient(const UserResource &ur) const
{
	// passed name is expected to be in lower case
	QString res;
	QString name;
	if (ur.clientName().isEmpty()) {
		name = QUrl(ur.status().caps().node()).host();
	} else {
		name = ur.clientName().toLower();
	}
	if (!name.isEmpty()) {
		res = PsiIconset::instance()->caps2client(name);
		//qDebug("CLIENT: %s RES: %s", qPrintable(name), qPrintable(res));
	}
	if (res.isEmpty()) {
		res = "unknown";
		//qDebug("RESOURCE: %s RES: %s", qPrintable(ur.name()), qPrintable(res));
	}
	return res;
}

void UserListItem::setActivity(const Activity& activity)
{
	v_activity = activity;
}

const Activity& UserListItem::activity() const
{
	return v_activity;
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

/*void UserListItem::setPhysicalLocation(const PhysicalLocation& physicalLocation)
{
	v_physicalLocation = physicalLocation;
}

const PhysicalLocation& UserListItem::physicalLocation() const
{
	return v_physicalLocation;
}*/

void UserListItem::setAvatarFactory(AvatarFactory* av)
{
	v_avatarFactory = av;
}

void UserListItem::setJid(const Jid &j)
{
	LiveRosterItem::setJid(j);

	int n = jid().full().indexOf('@');
	if(n == -1)
		v_isTransport = true;
	else
		v_isTransport = false;
}

bool UserListItem::isTransport() const
{
	return v_isTransport;
}

bool UserListItem::isConference() const
{
	return v_isConference;
}

void UserListItem::setConference(bool b)
{
	v_isConference = b;
}

void UserListItem::setPending(int p, int h)
{
	v_pending = p;
	v_hPending = h;
}

QString UserListItem::pending() const
{
	QString str;
	if (v_hPending)
		str = QString("[%1/%2]").arg(v_pending).arg(v_hPending);
	else if (v_pending)
		str = QString("[%1]").arg(v_pending);
	return str;
}

bool UserListItem::isAvailable() const
{
	return !v_url.isEmpty();
}

bool UserListItem::isHidden() const
{
	return groups().contains(qApp->translate("PsiContact", "Hidden"));
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
	// NOTE: If you add something to the tooltip,
	// you most probably want to wrap it with TextUtil::escape()

	QString str;
	int s = PsiIconset::instance()->system().iconSize();
	str +=QString("<style type='text/css'> \
		.layer1 { white-space:pre; margin-left:%1px;} \
		.layer2 { white-space:normal; margin-left:%1px;} \
	</style>").arg(s+2);

	QString imgTag = "icon name"; // or 'img src' if appropriate QMimeSourceFactory is installed. but mblsha noticed that QMimeSourceFactory unloads sometimes
	bool useAvatar = false;
	bool mucItem = false;

	if(!userResourceList().isEmpty()) {
		mucItem = userResourceList()[0].status().hasMUCItem();
	}

	if (v_avatarFactory && !v_avatarFactory->getAvatar(jid().bare()).isNull() && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.avatar").toBool())
		useAvatar = true;

	str += "<table cellspacing=\"3\"><tr>";
	str += "<td>";

	if (useAvatar) {
		str += QString("<icon name=\"avatars/%1\">").arg(jid().bare());
		str += "</td><td width=\"10\"></td>";
		str += "<td>";
	}

	QString nick = JIDUtil::nickOrJid(name(), jid().full());
	if (!mucItem) {
		if(jid().full() != nick)
			str += QString("<div style='white-space:pre'>%1 &lt;%2&gt;</div>").arg(TextUtil::escape(nick)).arg(TextUtil::escape(JIDUtil::toString(jid(),true)));
		else
			str += QString("<div style='white-space:pre'>%1</div>").arg(TextUtil::escape(nick));
	}

	// subscription
	if(!v_self && !v_isConference && subscription().type() != Subscription::Both && !mucItem)
		str += QString("<div style='white-space:pre'>") + QObject::tr("Subscription") + ": " + subscription().toString() + "</div>";

	if(!v_keyID.isEmpty() && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.pgp").toBool())
		str += QString("<div style='white-space:pre'><%1=\"%2\"> ").arg(imgTag).arg("psi/pgp") + QObject::tr("OpenPGP") + ": " + v_keyID.right(8) + "</div>";

	// User Mood
	if (!mood().isNull()) {
		str += QString("<div style='white-space:pre'><%1=\"mood/%2\"> ").arg(imgTag).arg(mood().typeValue()) + QObject::tr("Mood") + ": " + mood().typeText();
		if (!mood().text().isEmpty())
			str += QString(" (") + TextUtil::escape(mood().text()) + QString(")");
		str += "</div>";
	}

	// User Activity
	if (!activity().isNull()) {
		str += QString("<div style='white-space:pre'><%1=\"%2\"> ").arg(imgTag).arg(activityIconName(activity())) + 
			QObject::tr("Activity") + ": " + activity().typeText();
		if (activity().specificType() != Activity::UnknownSpecific) {
			str += QString(" - ") + activity().specificTypeText();
		}
		if (!activity().text().isEmpty())
			str += QString(" (") + TextUtil::escape(activity().text()) + QString(")");
		str += "</div>";
	}

	// User Tune
	if (!tune().isEmpty())
		str += QString("<div style='white-space:pre'><%1=\"%2\"> ").arg(imgTag).arg("psi/notification_roster_tune") + QObject::tr("Listening to") + ": " + TextUtil::escape(tune()) + "</div>";

	// User Physical Location
	//if (!physicalLocation().isNull())
	//	str += QString("<div style='white-space:pre'>") + QObject::tr("Location") + ": " + TextUtil::escape(physicalLocation().toString()) + "</div>";

	// User Geolocation
	if (!geoLocation().isNull() && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.geolocation").toBool())
		str += QString("<div style='white-space:pre'><table cellspacing=\"0\"><tr><td><%1=\"%2\"> </td><td><div>%3</div></td></tr></table></div>") \
		.arg(imgTag).arg("system/geolocation").arg(TextUtil::escape(geoLocation().toString().trimmed()));

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

			QString secstr;
			if(isSecure(r.name()) && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.pgp").toBool())
				secstr += QString(" <%1=\"psi/cryptoYes\">").arg(imgTag);
			QString hr;
			if (!mucItem)
				hr = "<hr/>";
			str += hr + "<div style='white-space:pre'>";

			PsiIcon *statusIcon = PsiIconset::instance()->statusPtr(jid(), makeSTATUS(r.status()));
			if (statusIcon) {
				QByteArray imageArray;
				QBuffer buff(&imageArray);
				statusIcon->image().save(&buff, "png");
				QString imgBase64(QUrl::toPercentEncoding(imageArray.toBase64()));
				str += QString("<img src=\"data:image/png;base64,%1\" alt=\"img\"/>").arg(imgBase64);
			}

			str += QString(" <b>%1</b> ").arg(TextUtil::escape(name)) + QString("(%1)").arg(r.priority());
			if (!r.status().mucItem().jid().isEmpty())
				str += QString(" &lt;%1&gt;").arg(TextUtil::escape(JIDUtil::toString(r.status().mucItem().jid(),true)));
			str += secstr + "</div>";

			if(!r.publicKeyID().isEmpty() && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.pgp").toBool()) {
				int v = r.pgpVerifyStatus();
				if(v == QCA::SecureMessageSignature::Valid || v == QCA::SecureMessageSignature::InvalidSignature || v == QCA::SecureMessageSignature::InvalidKey || v == QCA::SecureMessageSignature::NoKey) {
					if(v == QCA::SecureMessageSignature::Valid) {
						QString d = r.sigTimestamp().toString(Qt::DefaultLocaleShortDate);
						str += QString("<div class='layer1'><%1=\"%2\"> ").arg(imgTag).arg("psi/gpg-yes") + QObject::tr("Signed") + ": " + "<font color=\"#2A993B\">" + d + "</font>";
					}
					else if(v == QCA::SecureMessageSignature::NoKey) {
						QString d = r.sigTimestamp().toString(Qt::DefaultLocaleShortDate);
						str += QString("<div class='layer1'><%1=\"%2\"> ").arg(imgTag).arg("psi/keyUnknown") + QObject::tr("Signed") + ": " + d;
					}
					else if(v == QCA::SecureMessageSignature::InvalidSignature || v == QCA::SecureMessageSignature::InvalidKey) {
						str += QString("<div class='layer1'><%1=\"%2\"> ").arg(imgTag).arg("psi/keyBad") + "<font color=\"#810000\">" + QObject::tr("Bad signature") + "</font>";
					}

					if(v_keyID != r.publicKeyID())
						str += QString(" [%1]").arg(r.publicKeyID().right(8));
					str += "</div>";
				}
			}

			// client
			if(!r.versionString().isEmpty() && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.client-version").toBool()) {
				QString ver = r.versionString();
				if(trim)
					ver = dot_truncate(ver, 80);
				ver = TextUtil::escape(ver);
				str += QString("<div class='layer1'><%1=\"%2\"> ").arg(imgTag).arg("clients/" + findClient(r)) + QObject::tr("Using") + QString(": %3").arg(ver) + "</div>";
			}


			// Entity Time
			if (r.timezoneOffset().hasValue()) {
				QDateTime dt = QDateTime::currentDateTime().toUTC().addSecs(r.timezoneOffset().value()*60);
				str += QString("<div class='layer1'><%1=\"%2\"> ").arg(imgTag).arg("psi/time") + QObject::tr("Time") + QString(": %1 (%2)").arg(dt.toString(Qt::DefaultLocaleShortDate)).arg(r.timezoneOffsetString()) + "</div>";
			}

			// MUC
			if(!v_isConference && r.status().hasMUCItem()) {
				MUCItem::Affiliation a = r.status().mucItem().affiliation();
				QString aff;
				if(a == MUCItem::Owner)
					aff = "affiliation/owner";
				else if(a == MUCItem::Admin)
					aff = "affiliation/admin";
				else if(a == MUCItem::Member)
					aff = "affiliation/member";
				else if(a == MUCItem::Outcast)
					aff = "affiliation/outcast";
				else
					aff = "affiliation/noaffiliation";
				//if(!r.status().mucItem().jid().isEmpty())
				//	str += QString("<div class='layer1'>") + QObject::tr("JID: %1").arg(JIDUtil::toString(r.status().mucItem().jid(),true)) + QString("</div>");
				if(r.status().mucItem().role() != MUCItem::NoRole)
					str += QString("<div class='layer2'><table cellspacing=\"0\"><tr><td><%1=\"%2\"> </td><td>").arg(imgTag).arg(aff);
					str += QString("<div style='white-space:pre'>") + QObject::tr("Role: %1").arg(MUCManager::roleToString(r.status().mucItem().role())) + QString("</div>");
					str += QString("<div style='white-space:pre'>") + QObject::tr("Affiliation: %1").arg(MUCManager::affiliationToString(r.status().mucItem().affiliation())) + QString("</td></tr></table></div>");
			}

			// last status
			if(r.status().timeStamp().isValid() && PsiOptions::instance()->getOption("options.ui.contactlist.tooltip.last-status").toBool()) {
				QString d = r.status().timeStamp().toString(Qt::DefaultLocaleShortDate);
				str += QString("<div class='layer1'><%1=\"%2\"> ").arg(imgTag).arg("psi/info") + QObject::tr("Last Status") + ": " + d + "</div>";
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
				if( PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool() && !doLinkify )
					s = TextUtil::emoticonify(s);
				if( !doLinkify && PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool() )
					s = TextUtil::legacyFormat(s);

				str += QString("<div class='layer2'><table cellspacing=\"0\"><tr><td><%1=\"%2\"> </td><td><div><u>%3</u>: %4</div></td></tr></table></div>") \
				.arg(imgTag).arg("psi/action_templates_edit").arg(head).arg(s);
			}
		}
	}
	else {
		// last available
		if(!lastAvailable().isNull()) {
			QString d = lastAvailable().toString(Qt::DefaultLocaleShortDate);
			str += QString("<div style='white-space:pre'><%1=\"%2\"> ").arg(imgTag).arg("psi/info") + QObject::tr("Last Available") + ": " + d + "</div>";
		}

		// presence error
		if(!v_perr.isEmpty()) {
			QStringList err = v_perr.split('\n');
			str += QString("<div style='white-space:pre'>") + QObject::tr("Presence Error") + QString(": %1").arg(TextUtil::escape(err[0])) + "</div>";
			err.pop_front();
			foreach (QString line, err)
				str += "<div>" + TextUtil::escape(line) + "</div>";
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
			str += QString("<div class='layer2'><table cellspacing=\"0\"><tr><td><%1=\"%2\"> </td><td><div><u>%3</u>: %4</div></td></tr></table></div>") \
			.arg(imgTag).arg("psi/action_templates_edit").arg(head).arg(s);
		}
	}

	str += "</td>";
	str += "</tr></table>";

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

bool UserListItem::isSecure() const
{
	return !secList.isEmpty();
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
	foreach(const QString s, secList) {
		if(s == rname) {
			if(!b)
				secList.removeAll(s);
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
	foreach(UserListItem* i, *this) {
		if(i->jid().compare(j))
			return i;
	}
	return 0;
}

