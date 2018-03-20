#include <QApplication>
#include "mediadevicewatcher.h"
#include "../psimedia/psimedia.h"
#include "psioptions.h"

MediaDeviceWatcher::MediaDeviceWatcher(QObject *parent) : QObject(parent)
{
    connect(&_features, SIGNAL(updated()), SLOT(featuresUpdated()));
    updateDefaults();
}

MediaDeviceWatcher* MediaDeviceWatcher::_instance = nullptr;
MediaDeviceWatcher* MediaDeviceWatcher::instance()
{
    if (!_instance)
        _instance = new MediaDeviceWatcher(qApp);
    return _instance;
}


void MediaDeviceWatcher::updateDefaults()
{
    QString id;

    QString userPrefAudioOut = PsiOptions::instance()->getOption("options.media.devices.audio-output").toString();
    bool hasAudioOut = !id.isNull();
    QString userPrefAudioIn = PsiOptions::instance()->getOption("options.media.devices.audio-input").toString();
    bool hasAudioIn = !id.isNull();
    QString userPrefVideoIn = PsiOptions::instance()->getOption("options.media.devices.video-input").toString();
    bool hasVideoIn = !id.isNull();


    //configuration.liveInput = s.value("liveInput", true).toBool();
	//configuration.loopFile = s.value("liveFile", true).toBool();
    //configuration.file = s.value("file", QString()).toString();

    //QString audioParams = s.value("audioParams").toString();
    //QString videoParams = s.value("videoParams").toString();

    _configuration.audioOutDeviceId = (hasAudioIn && userPrefAudioIn.isEmpty())?
                QString() : defaultDeviceId(_features.audioOutputDevices(), userPrefAudioOut);
    _configuration.audioInDeviceId = (hasAudioOut && userPrefAudioOut.isEmpty())?
                QString() : defaultDeviceId(_features.audioInputDevices(), userPrefAudioIn);
    _configuration.videoInDeviceId = (hasVideoIn && userPrefVideoIn.isEmpty())?
                QString() : defaultDeviceId(_features.videoInputDevices(), userPrefVideoIn);
}

void MediaDeviceWatcher::featuresUpdated()
{
    updateDefaults();
    emit updated();
}

QString MediaDeviceWatcher::defaultDeviceId(const QList<PsiMedia::Device> &devs, const QString &userPref)
{
    QString def;
    bool userPrefFound = false;
    for (auto const &d: devs) {
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
