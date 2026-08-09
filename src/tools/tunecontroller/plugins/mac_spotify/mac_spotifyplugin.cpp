/*
 * mac_spotifyplugin.cpp
 * Copyright (C) 2006  Remko Troncon
 * Copyright (C) 2026  Taylor Fox
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "macdnctunecontroller.h"
#include "tunecontrollerplugin.h"

#include <QObject>
#include <QString>
#include <QtCore>

class MacSpotifyPlugin : public QObject, public TuneControllerPlugin {
    Q_OBJECT
    Q_INTERFACES(TuneControllerPlugin)
    Q_PLUGIN_METADATA(IID "org.psi-im.Psi.TuneControllerPlugin")

public:
    virtual QString         name();
    virtual TuneController *createController();
};

QString MacSpotifyPlugin::name() { return "Spotify (Mac)"; }

TuneController *MacSpotifyPlugin::createController() { return new MacDNCController(CFSTR("com.spotify.client.PlaybackStateChanged")); }

#include "mac_spotifyplugin.moc"
