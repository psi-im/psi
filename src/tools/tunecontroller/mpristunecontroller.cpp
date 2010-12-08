/*
 * mpristunecontroller.cpp 
 * Copyright (C) 2010 Vitaly Tonkacheyev
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

#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusConnectionInterface>

#include "mpristunecontroller.h"

/**
 * \class MPRISTuneController
 * \brief A common controller class for MPRIS compilant players.
 */

const char *MPRISTuneController::MPRIS_PREFIX = "org.mpris";
static QDBusConnection bus = QDBusConnection::sessionBus();

QDBusArgument &operator<<(QDBusArgument& arg, const PlayerStatus& ps)
{
	arg.beginStructure();
	arg << ps.playStatus;
	arg << ps.playOrder;
	arg << ps.playRepeat;
	arg << ps.stopOnce;
	arg.endStructure();
	return arg;
}

const QDBusArgument &operator>>(const QDBusArgument& arg, PlayerStatus& ps)
{
	arg.beginStructure();
	arg >> ps.playStatus;
	arg >> ps.playOrder;
	arg >> ps.playRepeat;
	arg >> ps.stopOnce;
	arg.endStructure();
	return arg;
}

MPRISTuneController::MPRISTuneController()
{
	qDBusRegisterMetaType<PlayerStatus>();
	players = bus.interface()->registeredServiceNames().value()
													   .filter(MPRIS_PREFIX);
	foreach(const QString &player, players){
		connectToBus(player);
	}
	bus.connect(QLatin1String("org.freedesktop.DBus"),
		    QLatin1String("/org/freedesktop/DBus"),
		    QLatin1String("org.freedesktop.DBus"),
		    QLatin1String("NameOwnerChanged"),
		    this,
		    SLOT(checkMprisService(QString, QString, QString)));
}

MPRISTuneController::~MPRISTuneController()
{
	foreach(const QString &player, players) {
		disconnectFromBus(player);
	}
	bus.disconnect(QLatin1String("org.freedesktop.DBus"),
		       QLatin1String("/org/freedesktop/DBus"),
		       QLatin1String("org.freedesktop.DBus"),
		       QLatin1String("NameOwnerChanged"),
		       this,
		       SLOT(checkMprisService(QString, QString, QString)));
}

void MPRISTuneController::checkMprisService(const QString &name,
					     const QString &oldOwner,
					     const QString &newOwner)
{
	Q_UNUSED(oldOwner);
	if (name.startsWith(MPRIS_PREFIX)) {
		int playerIndex = players.indexOf(name);
		if (playerIndex == -1) {
			if (!newOwner.isEmpty()) {
				players.append(name);
				connectToBus(name);
			}
		}
		else if (newOwner.isEmpty()) {
			disconnectFromBus(name);
			players.removeAt(playerIndex);
		}
	}
}

int MPRISTuneController::version(const QString &service_) const
{
	return service_.contains("MediaPlayer2") ? MPRIS_2 : MPRIS_1;
}

void MPRISTuneController::connectToBus(const QString &service_)
{
	if (version(service_) != MPRIS_2) {
		bus.connect(service_,
			    QLatin1String("/Player"),
			    QLatin1String("org.freedesktop.MediaPlayer"),
			    QLatin1String("StatusChange"),
			    QLatin1String("(iiii)"),
			    this,
			    SLOT(onPlayerStatusChange(PlayerStatus)));
		bus.connect(service_,
			    QLatin1String("/Player"),
			    QLatin1String("org.freedesktop.MediaPlayer"),
			    QLatin1String("TrackChange"),
			    QLatin1String("a{sv}"),
			    this,
			    SLOT(onTrackChange(QVariantMap)));
	}
	else {
		bus.connect(service_,
			    QLatin1String("/org/mpris/MediaPlayer2"),
			    QLatin1String("org.freedesktop.DBus.Properties"),
			    QLatin1String("PropertiesChanged"),
			    this,
			    SLOT(onPropertyChange(QDBusMessage)));
	}
}

void MPRISTuneController::disconnectFromBus(const QString &service_)
{
	if (version(service_) != MPRIS_2) {
		bus.disconnect(service_,
			       QLatin1String("/Player"),
			       QLatin1String("org.freedesktop.MediaPlayer"),
			       QLatin1String("StatusChange"),
			       QLatin1String("(iiii)"),
			       this,
			       SLOT(onPlayerStatusChange(PlayerStatus)));
		bus.disconnect(service_,
			       QLatin1String("/Player"),
			       QLatin1String("org.freedesktop.MediaPlayer"),
			       QLatin1String("TrackChange"),
			       QLatin1String("a{sv}"),
			       this,
			       SLOT(onTrackChange(QVariantMap)));
	}
	else {
		bus.disconnect(service_,
			       QLatin1String("/org/mpris/MediaPlayer2"),
			       QLatin1String("org.freedesktop.DBus.Properties"),
			       QLatin1String("PropertiesChanged"),
			       this,
			       SLOT(onPropertyChange(QDBusMessage)));
		if (whatPlaying == MPRIS_2) {
			emit stopped();
		}
	}
}

void MPRISTuneController::onPlayerStatusChange(const PlayerStatus &ps)
{
	if (ps.playStatus != StatusPlaying) {
		emit stopped();
	}
	else if (!currentTune_.isNull()) {
		emit playing(currentTune_);
	}
}

void MPRISTuneController::onTrackChange(const QVariantMap &map)
{
	whatPlaying = MPRIS_1;
	Tune tune = getTune(map);
	if (tune != currentTune_) {
		currentTune_ = tune;
		if (!currentTune_.isNull()) {
			emit playing(currentTune_);
		}
	}
}

void MPRISTuneController::onPropertyChange(const QDBusMessage &msg)
{
	QDBusArgument arg = msg.arguments().at(1).value<QDBusArgument>();
	QVariantMap map = qdbus_cast<QVariantMap>(arg);
	QVariant v = map.value(QLatin1String("Metadata"));
	if (v.isValid()) {
		whatPlaying = MPRIS_2;
		arg = v.value<QDBusArgument>();
		Tune tune = getMpris2Tune(qdbus_cast<QVariantMap>(arg));
		if (tune != currentTune_) {
			currentTune_ = tune;
			if (!currentTune_.isNull()) {
				emit playing(currentTune_);
			}
		}
	}
	v = map.value(QLatin1String("PlaybackStatus"));
	if (v.isValid()) {
		PlayerStatus status;
		status.playStatus = bool(v.toString() == QLatin1String("Playing")) ?
							StatusPlaying : StatusStopped;
		onPlayerStatusChange(status);
	}
}

Tune MPRISTuneController::currentTune()
{
	return currentTune_;
}

Tune MPRISTuneController::getTune(const QVariantMap &map) const
{
	Tune tune;
	tune.setName(map.value("title").toString());
	tune.setArtist(map.value("artist").toString());
	tune.setAlbum(map.value("album").toString());
	tune.setTrack(map.value("track").toString());
	tune.setTime(map.value("time").toUInt());
	return tune;
}

Tune MPRISTuneController::getMpris2Tune(const QVariantMap &map) const
{
	Tune tune;
	tune.setName(map.value("xesam:title").toString());
	tune.setArtist(map.value("xesam:artist").toString());
	tune.setAlbum(map.value("xesam:album").toString());
	tune.setTrack(QVariant(map.value("xesam:trackNumber").toUInt()).toString());
	tune.setTime(QVariant(map.value("mpris:length").toLongLong() / 1000000).toUInt());
	return tune;
}
