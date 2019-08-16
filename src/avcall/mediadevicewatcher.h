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

#ifndef MEDIADEVICEWATCHER_H
#define MEDIADEVICEWATCHER_H

#include "../psimedia/psimedia.h"

#include <QObject>

class MediaConfiguration
{
public:
    bool liveInput = false;
    QString audioOutDeviceId, audioInDeviceId, videoInDeviceId;
    QString file;
    bool loopFile = false;
    PsiMedia::AudioParams audioParams;
    PsiMedia::VideoParams videoParams;

    int basePort = -1;
    QString extHost;
};

class MediaDeviceWatcher : public QObject
{
    Q_OBJECT

    explicit MediaDeviceWatcher(QObject *parent = nullptr);
    QString defaultDeviceId(const QList<PsiMedia::Device> &devs, const QString &userPref);

public:
    static MediaDeviceWatcher* instance();
    void updateDefaults();
    inline const MediaConfiguration &configuration() const { return _configuration; }

    inline QList<PsiMedia::Device> audioInputDevices() { return _features.audioInputDevices(); }
    inline QList<PsiMedia::Device> audioOutputDevices() { return _features.audioOutputDevices(); }
    inline QList<PsiMedia::Device> videoInputDevices() { return _features.videoInputDevices(); }
    inline QList<PsiMedia::AudioParams> supportedAudioModes() { return _features.supportedAudioModes(); }
    inline QList<PsiMedia::VideoParams> supportedVideoModes() { return _features.supportedVideoModes(); }

signals:
    void updated();

public slots:

private slots:
    void featuresUpdated();

private:
    MediaConfiguration _configuration;
    PsiMedia::Features _features;
    static MediaDeviceWatcher *_instance;

};

#endif // MEDIADEVICEWATCHER_H
