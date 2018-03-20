#ifndef MEDIADEVICEWATCHER_H
#define MEDIADEVICEWATCHER_H

#include <QObject>
#include "../psimedia/psimedia.h"

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
