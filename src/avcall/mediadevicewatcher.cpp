/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
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

#include "mediadevicewatcher.h"

#include "../psimedia/psimedia.h"
#include "psioptions.h"

#include <QApplication>

MediaDeviceWatcher::MediaDeviceWatcher(QObject *parent) : QObject(parent) { }

MediaDeviceWatcher *MediaDeviceWatcher::_instance = nullptr;
MediaDeviceWatcher *MediaDeviceWatcher::instance()
{
    if (!_instance)
        _instance = new MediaDeviceWatcher(qApp);
    return _instance;
}

void MediaDeviceWatcher::selectDevices(const QString &audioInput, const QString &audioOutput, const QString &videoInput)
{
    _configuration.audioOutDeviceId
        = audioOutput.isEmpty() ? QString() : defaultDeviceId(_features.audioOutputDevices(), audioOutput);
    _configuration.audioInDeviceId
        = audioInput.isEmpty() ? QString() : defaultDeviceId(_features.audioInputDevices(), audioInput);
    _configuration.videoInDeviceId
        = videoInput.isEmpty() ? QString() : defaultDeviceId(_features.videoInputDevices(), videoInput);
    emit updated();
}

QString MediaDeviceWatcher::defaultDeviceId(const QList<PsiMedia::Device> &devs, const QString &userPref)
{
    QString def;
    bool    userPrefFound = false;
    for (auto const &d : devs) {
        if (d.isDefault())
            def = d.id();
        if (d.id() == userPref) {
            userPrefFound = true;
            break;
        }
    }

    if (userPrefFound)
        return userPref;

    if (def.isEmpty() && !devs.isEmpty()) {
        def = devs.first().id();
    }

    return def;
}
