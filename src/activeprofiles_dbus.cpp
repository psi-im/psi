/*
 * activeprofiles_dbus.cpp - Class for interacting with other psi instances
 * Copyright (C) 2006-2007  Martin Hostettler
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

#include "activeprofiles.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>


#include <QLabel>
#include <QTimer>


#include "applicationinfo.h"
#include "dbus.h"





/** \brief encodes a string to "[A-Z][a-z][0-9]_-" ascii subset
 * [A-Z][a-z][0-9] -> [A-Z][a-z][0-9]
 * / -> _
 * everything else to "-XX" with XX hex code of char
 * (slow)
 */
static QString encodeAlNumD(QString in)
{
	QString out;
	QByteArray chars = in.toUtf8();
	bool first = true;
	foreach(char c, chars) {
		if (('A' <= c) && (c <= 'z')) {
			out += (char)c;
		} else if (('0' <= c) && (c <= '9') && !first) {
			out += (char)c;
		} else if ('/' == c) {
			out += "_";
		} else {
			out += QString("-%1").arg(c, 2, 16, QChar('0'));
		}
		first = false;
	}
	return out;
}


/** \brief DBus busname registration helper
 * \param dbusIface interface of bus
 * \param name busname to register
 * \param queue try queueing? 
 * \return got dbus name?
 */
static bool registerBusname(QDBusConnectionInterface *dbusIface, QString name, bool queue)
{
	bool ok = false;
	QDBusReply<QDBusConnectionInterface::RegisterServiceReply> reply;
	reply = dbusIface->registerService(name, 
		queue ?	QDBusConnectionInterface::QueueService : QDBusConnectionInterface::DontQueueService,
			QDBusConnectionInterface::AllowReplacement);
	if (reply.isValid()) {
		switch (reply.value()) {
			case QDBusConnectionInterface::ServiceNotRegistered:
				qWarning("failed to register dbus name %s:", qPrintable(name)); 
				break;
			case QDBusConnectionInterface::ServiceQueued:
				qDebug("dbus name %s already taken, queueing", qPrintable(name));
				break;
			case QDBusConnectionInterface::ServiceRegistered:
				ok = true;
		}
	} else {
		qWarning("failed to register dbus name %s: %s", qPrintable(name), qPrintable(reply.error().message()));
	}
	return ok;
}


class ActiveProfiles::Private {
public:
	QString profile;
	QStringList busNames;
	bool registerBusnames(QString prof);

	QString dbusName(QString prof);
};


QString ActiveProfiles::Private::dbusName(QString prof)
{
	QString name = PSIDBUSNAME;
	name += ".";
	name += encodeAlNumD(ApplicationInfo::homeDir()).right(qMax(0,200-name.size()));
	if (!prof.isEmpty()) {
		name += ".";
		name += encodeAlNumD(prof).right(qMax(0,250-name.size()));
	}
	return name;
}



bool ActiveProfiles::Private::registerBusnames(QString prof)
{
	// FIXME move where?
	if (!QDBusConnection::sessionBus().isConnected()) {
		qWarning("can't connect to dbus");
		return true;
	}


	QDBusConnectionInterface *dbusIface = QDBusConnection::sessionBus().interface ();

	QString name = PSIDBUSNAME;
	registerBusname(dbusIface, name, true);
	busNames << name;
	name = dbusName(QString());
	registerBusname(dbusIface, name, true);
	busNames << name;
	name = dbusName(prof);
	busNames << name;
	return registerBusname(dbusIface, name, false);

}


bool ActiveProfiles::isActive(const QString &profile) const
{
	if (!QDBusConnection::sessionBus().isConnected()) {
		qWarning("can't connect to dbus");
		return false;
	}

	QDBusConnectionInterface *dbusIface = QDBusConnection::sessionBus().interface ();
	return dbusIface->isServiceRegistered(d->dbusName(profile));
}

bool ActiveProfiles::isAnyActive() const
{
	return isActive("");
}


bool ActiveProfiles::setThisProfile(const QString &profile)
{
	if (profile == d->profile)
		return true;

	if (profile.isEmpty()) {
		unsetThisProfile();
		return true;
	}
	bool ok = d->registerBusnames(profile);
	if (ok) {
		d->profile = profile;
	} else {
		unsetThisProfile();
	}
	return ok;
}

void ActiveProfiles::unsetThisProfile()
{
	QDBusConnectionInterface *dbusIface = QDBusConnection::sessionBus().interface ();
	foreach(QString name, d->busNames) {
		dbusIface->unregisterService(name);
	}
	d->busNames.clear();
	d->profile = QString::null;
}

QString ActiveProfiles::thisProfile() const
{
	return d->profile;
}

ActiveProfiles::ActiveProfiles()
	: QObject(QCoreApplication::instance())
{
	d = new ActiveProfiles::Private;
}

ActiveProfiles::~ActiveProfiles()
{
	delete d;
	d = 0;
}

bool ActiveProfiles::setStatus(const QString &profile, const QString &status, const QString &message) const
{
	QDBusInterface(d->dbusName(profile), "/Main", PSIDBUSMAINIF).call(QDBus::NoBlock,
			"setStatus", status, message);
	return true;
}

bool ActiveProfiles::openUri(const QString &profile, const QString &uri) const
{
	QDBusInterface(d->dbusName(profile), "/Main", PSIDBUSMAINIF).call(QDBus::NoBlock, 
			"openURI", uri);
	return true;
}

bool ActiveProfiles::raise(const QString &profile, bool withUI) const
{
	QLabel *lab=0;
	QDBusMessage msg = QDBusMessage::createMethodCall ( d->dbusName(profile), "/Main", PSIDBUSMAINIF, "raise" );
	if (withUI) {
		lab = new QLabel(tr("This psi profile is already running...<br>please wait..."));
		QTimer::singleShot(250, lab, SLOT(show()));
	}
	QDBusMessage rmsg = QDBusConnection::sessionBus().call(msg, QDBus::BlockWithGui, 10000);
	if (withUI) {
		lab->hide();
		delete lab;
	}
	if (rmsg.type() == QDBusMessage::ReplyMessage) {
		return true;
	} else return false;
}
