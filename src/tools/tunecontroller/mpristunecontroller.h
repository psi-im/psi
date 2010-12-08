/*
 * mpristunecontroller.h
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

#ifndef MPRISTUNECONTROLLER_H
#define MPRISTUNECONTROLLER_H

#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusInterface>
#include <QVariantMap>
#include <QStringList>

#include "tunecontrollerinterface.h"
#include "tune.h"

struct PlayerStatus
{
	int playStatus;
	int playOrder;
	int playRepeat;
	int stopOnce;
};

Q_DECLARE_METATYPE(PlayerStatus)

class MPRISTuneController : public TuneController
{
	Q_OBJECT

	static const char *MPRIS_PREFIX;

	enum MPRISVersion {
		MPRIS_1 = 1,
		MPRIS_2 = 2
	};

	enum PlayStatus {
		StatusPlaying = 0,
		StatusStopped = 1
	};

public:
	MPRISTuneController();
	~MPRISTuneController();

	Tune currentTune();

protected slots:
	void checkMprisService(const QString &name, const QString &oldOwner, const QString &newOwner);
	void onTrackChange(const QVariantMap &map);
	void onPlayerStatusChange(const PlayerStatus &ps);
	void onPropertyChange(const QDBusMessage &msg);
private:
	Tune getTune(const QVariantMap &map) const;
	Tune getMpris2Tune(const QVariantMap &map) const;
	int version(const QString &service_) const;
	void connectToBus(const QString &service_);
	void disconnectFromBus(const QString &service_);

private:
	Tune currentTune_;
	QStringList players;
	int whatPlaying;
};

#endif
