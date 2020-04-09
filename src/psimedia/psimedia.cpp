/*
 * Copyright (C) 2008-2009  Barracuda Networks, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "psimedia_p.h"

#include <QMetaMethod>

namespace PsiMedia {
static AudioParams importAudioParams(const PAudioParams &pp)
{
    AudioParams out;
    out.setCodec(pp.codec);
    out.setSampleRate(pp.sampleRate);
    out.setSampleSize(pp.sampleSize);
    out.setChannels(pp.channels);
    return out;
}

static PAudioParams exportAudioParams(const AudioParams &p)
{
    PAudioParams out;
    out.codec      = p.codec();
    out.sampleRate = p.sampleRate();
    out.sampleSize = p.sampleSize();
    out.channels   = p.channels();
    return out;
}

static VideoParams importVideoParams(const PVideoParams &pp)
{
    VideoParams out;
    out.setCodec(pp.codec);
    out.setSize(pp.size);
    out.setFps(pp.fps);
    return out;
}

static PVideoParams exportVideoParams(const VideoParams &p)
{
    PVideoParams out;
    out.codec = p.codec();
    out.size  = p.size();
    out.fps   = p.fps();
    return out;
}

static PayloadInfo importPayloadInfo(const PPayloadInfo &pp)
{
    PayloadInfo out;
    out.setId(pp.id);
    out.setName(pp.name);
    out.setClockrate(pp.clockrate);
    out.setChannels(pp.channels);
    out.setPtime(pp.ptime);
    out.setMaxptime(pp.maxptime);
    QList<PayloadInfo::Parameter> list;
    foreach (const PPayloadInfo::Parameter &pi, pp.parameters) {
        PayloadInfo::Parameter i;
        i.name  = pi.name;
        i.value = pi.value;
        list += i;
    }
    out.setParameters(list);
    return out;
}

static PPayloadInfo exportPayloadInfo(const PayloadInfo &p)
{
    PPayloadInfo out;
    out.id        = p.id();
    out.name      = p.name();
    out.clockrate = p.clockrate();
    out.channels  = p.channels();
    out.ptime     = p.ptime();
    out.maxptime  = p.maxptime();
    QList<PPayloadInfo::Parameter> list;
    foreach (const PayloadInfo::Parameter &i, p.parameters()) {
        PPayloadInfo::Parameter pi;
        pi.name  = i.name;
        pi.value = i.value;
        list += pi;
    }
    out.parameters = list;
    return out;
}

//----------------------------------------------------------------------------
// Global
//----------------------------------------------------------------------------
static Provider *     g_provider     = nullptr;
static QPluginLoader *g_pluginLoader = nullptr;

static void cleanupProvider();

Provider *provider()
{
    if (!g_provider) {
        // static plugin around?
        Provider *  provider = nullptr;
        QObjectList list     = QPluginLoader::staticInstances();
        foreach (QObject *obj, list) {
            Plugin *instance = qobject_cast<Plugin *>(obj);
            if (!instance)
                continue;

            Provider *p = instance->createProvider();
            if (p) {
                provider = p;
                break;
            }
        }

        if (provider) {
            if (!provider->init(QString())) {
                delete provider;
                return nullptr;
            }

            g_provider = provider;
            qAddPostRoutine(cleanupProvider);
        }
    }

    return g_provider;
}

bool isSupported() { return (provider() ? true : false); }

PluginResult loadPlugin(const QString &fname, const QString &resourcePath)
{
    if (g_provider)
        return PluginSuccess;

    QPluginLoader *loader = new QPluginLoader(fname);
    if (!loader->load()) {
        delete loader;
        return ErrorLoad;
    }

    Plugin *instance = qobject_cast<Plugin *>(loader->instance());
    if (!instance) {
        delete loader;
        return ErrorVersion;
    }

    Provider *provider = instance->createProvider();
    if (!provider) {
        loader->unload();
        delete loader;
        return ErrorInit;
    }

    if (!provider->init(resourcePath)) {
        delete provider;
        loader->unload();
        delete loader;
        return ErrorInit;
    }

    g_provider     = provider;
    g_pluginLoader = loader;
    qAddPostRoutine(cleanupProvider);
    return PluginSuccess;
}

void cleanupProvider()
{
    if (!g_provider)
        return;

    delete g_provider;
    g_provider = nullptr;

    if (g_pluginLoader) {
        g_pluginLoader->unload();
        delete g_pluginLoader;
        g_pluginLoader = nullptr;
    }
}

void unloadPlugin() { cleanupProvider(); }

QString creditName() { return provider()->creditName(); }

QString creditText() { return provider()->creditText(); }

class Device::Private {
public:
    Device::Type type;
    QString      id;
    QString      name;
    bool         isDefault = false;
};

class Global {
public:
    static Device importDevice(const PDevice &pd)
    {
        Device dev;
        dev.d            = new Device::Private;
        dev.d->type      = Device::Type(pd.type);
        dev.d->id        = pd.id;
        dev.d->name      = pd.name;
        dev.d->isDefault = pd.isDefault;
        return dev;
    }
};

//----------------------------------------------------------------------------
// Device
//----------------------------------------------------------------------------
Device::Device() : d(nullptr) {}

Device::Device(const Device &other) : d(other.d ? new Private(*other.d) : nullptr) {}

Device::~Device() { delete d; }

Device &Device::operator=(const Device &other)
{
    if (this == &other)
        return *this;

    if (d) {
        if (other.d) {
            *d = *other.d;
        } else {
            delete d;
            d = nullptr;
        }
    } else {
        if (other.d)
            d = new Private(*other.d);
    }

    return *this;
}

bool Device::isNull() const { return (d ? false : true); }

Device::Type Device::type() const { return d->type; }

QString Device::name() const { return d->name; }

QString Device::id() const { return d->id; }

bool Device::isDefault() const { return d->isDefault; }

#ifdef QT_GUI_LIB
//----------------------------------------------------------------------------
// VideoWidget
//----------------------------------------------------------------------------

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent) { d = new VideoWidgetPrivate(this); }

VideoWidget::~VideoWidget() { delete d; }

QSize VideoWidget::sizeHint() const { return d->videoSize; }

void VideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    emit     d->paintEvent(&p);
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    emit d->resized(size());
}
#endif

//----------------------------------------------------------------------------
// AudioParams
//----------------------------------------------------------------------------
class AudioParams::Private {
public:
    QString codec;
    int     sampleRate;
    int     sampleSize;
    int     channels;

    Private() : sampleRate(0), sampleSize(0), channels(0) {}
};

AudioParams::AudioParams() : d(new Private) {}

AudioParams::AudioParams(const AudioParams &other) : d(new Private(*other.d)) {}

AudioParams::~AudioParams() { delete d; }

AudioParams &AudioParams::operator=(const AudioParams &other)
{
    *d = *other.d;
    return *this;
}

QString AudioParams::codec() const { return d->codec; }

int AudioParams::sampleRate() const { return d->sampleRate; }

int AudioParams::sampleSize() const { return d->sampleSize; }

int AudioParams::channels() const { return d->channels; }

QString AudioParams::toString() const
{
    return QString("%1 %2 %3 %4")
        .arg(d->codec, QString::number(d->sampleRate), QString::number(d->sampleSize), QString::number(d->channels));
}

void AudioParams::setCodec(const QString &s) { d->codec = s; }

void AudioParams::setSampleRate(int n) { d->sampleRate = n; }

void AudioParams::setSampleSize(int n) { d->sampleSize = n; }

void AudioParams::setChannels(int n) { d->channels = n; }

bool AudioParams::operator==(const AudioParams &other) const
{
    if (d->codec == other.d->codec && d->sampleRate == other.d->sampleRate && d->sampleSize == other.d->sampleSize
        && d->channels == other.d->channels) {
        return true;
    } else
        return false;
}

//----------------------------------------------------------------------------
// VideoParams
//----------------------------------------------------------------------------
class VideoParams::Private {
public:
    QString codec;
    QSize   size;
    int     fps;

    Private() : fps(0) {}
};

VideoParams::VideoParams() : d(new Private) {}

VideoParams::VideoParams(const VideoParams &other) : d(new Private(*other.d)) {}

VideoParams::~VideoParams() { delete d; }

VideoParams &VideoParams::operator=(const VideoParams &other)
{
    *d = *other.d;
    return *this;
}

QString VideoParams::codec() const { return d->codec; }

QSize VideoParams::size() const { return d->size; }

int VideoParams::fps() const { return d->fps; }

void VideoParams::setCodec(const QString &s) { d->codec = s; }

void VideoParams::setSize(const QSize &s) { d->size = s; }

void VideoParams::setFps(int n) { d->fps = n; }

bool VideoParams::operator==(const VideoParams &other) const
{
    if (d->codec == other.d->codec && d->size == other.d->size && d->fps == other.d->fps) {
        return true;
    } else
        return false;
}

QString VideoParams::toString() const
{
    return QString("%1 %2 %3")
        .arg(d->codec, QString::number(d->size.width()) + "x" + QString::number(d->size.height()),
             QString::number(d->fps));
}

//----------------------------------------------------------------------------
// Features
//----------------------------------------------------------------------------
QList<Device> importDevices(const QList<PDevice> &in)
{
    QList<Device> out;
    foreach (const PDevice &pd, in)
        out += Global::importDevice(pd);
    return out;
}

QList<AudioParams> importAudioModes(const QList<PAudioParams> &in)
{
    QList<AudioParams> out;
    foreach (const PAudioParams &pp, in)
        out += importAudioParams(pp);
    return out;
}

QList<VideoParams> importVideoModes(const QList<PVideoParams> &in)
{
    QList<VideoParams> out;
    foreach (const PVideoParams &pp, in)
        out += importVideoParams(pp);
    return out;
}

Features::Features(QObject *parent) : QObject(parent) { d = new Private(this); }

Features::~Features() { delete d; }

QList<Device> Features::audioOutputDevices() { return d->audioOutputDevices; }

QList<Device> Features::audioInputDevices() { return d->audioInputDevices; }

QList<Device> Features::videoInputDevices() { return d->videoInputDevices; }

QList<AudioParams> Features::supportedAudioModes() { return d->supportedAudioModes; }

QList<VideoParams> Features::supportedVideoModes() { return d->supportedVideoModes; }

//----------------------------------------------------------------------------
// RtpPacket
//----------------------------------------------------------------------------
class RtpPacket::Private : public QSharedData {
public:
    QByteArray rawValue;
    int        portOffset;

    Private(const QByteArray &_rawValue, int _portOffset) : rawValue(_rawValue), portOffset(_portOffset) {}
};

RtpPacket::RtpPacket() : d(nullptr) {}

RtpPacket::RtpPacket(const QByteArray &rawValue, int portOffset) : d(new Private(rawValue, portOffset)) {}

RtpPacket::RtpPacket(const RtpPacket &other) : d(other.d) {}

RtpPacket::~RtpPacket() {}

RtpPacket &RtpPacket::operator=(const RtpPacket &other)
{
    d = other.d;
    return *this;
}

bool RtpPacket::isNull() const { return (d ? false : true); }

QByteArray RtpPacket::rawValue() const { return d->rawValue; }

int RtpPacket::portOffset() const { return d->portOffset; }

//----------------------------------------------------------------------------
// RtpChannel
//----------------------------------------------------------------------------
RtpChannel::RtpChannel() { d = new RtpChannelPrivate(this); }

RtpChannel::~RtpChannel() { delete d; }

int RtpChannel::packetsAvailable() const
{
    if (d->c)
        return d->c->packetsAvailable();
    else
        return 0;
}

RtpPacket RtpChannel::read()
{
    if (d->c) {
        PRtpPacket pp = d->c->read();
        return RtpPacket(pp.rawValue, pp.portOffset);
    } else
        return RtpPacket();
}

void RtpChannel::write(const RtpPacket &rtp)
{
    if (d->c) {
        if (!d->enabled) {
            d->enabled = true;
            d->c->setEnabled(true);
        }

        PRtpPacket pp;
        pp.rawValue   = rtp.rawValue();
        pp.portOffset = rtp.portOffset();
        d->c->write(pp);
    }
}

void RtpChannel::connectNotify(const QMetaMethod &signal)
{
    int oldtotal = d->readyReadListeners;

    if (signal == QMetaMethod::fromSignal(&RtpChannel::readyRead))
        ++d->readyReadListeners;

    int total = d->readyReadListeners;
    if (d->c && oldtotal == 0 && total > 0) {
        d->enabled = true;
        d->c->setEnabled(true);
    }
}

void RtpChannel::disconnectNotify(const QMetaMethod &signal)
{
    int oldtotal = d->readyReadListeners;

    if (signal == QMetaMethod::fromSignal(&RtpChannel::readyRead))
        --d->readyReadListeners;

    int total = d->readyReadListeners;
    if (d->c && oldtotal > 0 && total == 0) {
        d->enabled = false;
        d->c->setEnabled(false);
    }
}

//----------------------------------------------------------------------------
// PayloadInfo
//----------------------------------------------------------------------------
bool PayloadInfo::Parameter::operator==(const PayloadInfo::Parameter &other) const
{
    // according to xep-167, parameter names are case-sensitive
    if (name == other.name && value == other.value)
        return true;
    else
        return false;
}

class PayloadInfo::Private {
public:
    int                           id;
    QString                       name;
    int                           clockrate;
    int                           channels;
    int                           ptime;
    int                           maxptime;
    QList<PayloadInfo::Parameter> parameters;

    Private() : id(-1), clockrate(-1), channels(-1), ptime(-1), maxptime(-1) {}

    bool operator==(const Private &other) const
    {
        // according to xep-167, parameters are unordered
        if (id == other.id && name.compare(other.name, Qt::CaseInsensitive) && clockrate == other.clockrate
            && channels == other.channels && ptime == other.ptime && maxptime == other.maxptime
            && compareUnordered(parameters, other.parameters)) {
            return true;
        } else
            return false;
    }

    static bool compareUnordered(const QList<PayloadInfo::Parameter> &a, const QList<PayloadInfo::Parameter> &b)
    {
        if (a.count() != b.count())
            return false;

        // for every parameter in 'a'
        foreach (const PayloadInfo::Parameter &p, a) {
            // make sure it is found in 'b'
            if (!b.contains(p))
                return false;
        }

        return true;
    }
};

PayloadInfo::PayloadInfo() : d(new Private) {}

PayloadInfo::PayloadInfo(const PayloadInfo &other) : d(new Private(*other.d)) {}

PayloadInfo::~PayloadInfo() { delete d; }

PayloadInfo &PayloadInfo::operator=(const PayloadInfo &other)
{
    *d = *other.d;
    return *this;
}

bool PayloadInfo::isNull() const { return (d->id == -1); }

int PayloadInfo::id() const { return d->id; }

QString PayloadInfo::name() const { return d->name; }

int PayloadInfo::clockrate() const { return d->clockrate; }

int PayloadInfo::channels() const { return d->channels; }

int PayloadInfo::ptime() const { return d->ptime; }

int PayloadInfo::maxptime() const { return d->maxptime; }

QList<PayloadInfo::Parameter> PayloadInfo::parameters() const { return d->parameters; }

void PayloadInfo::setId(int i) { d->id = i; }

void PayloadInfo::setName(const QString &str) { d->name = str; }

void PayloadInfo::setClockrate(int i) { d->clockrate = i; }

void PayloadInfo::setChannels(int num) { d->channels = num; }

void PayloadInfo::setPtime(int i) { d->ptime = i; }

void PayloadInfo::setMaxptime(int i) { d->maxptime = i; }

void PayloadInfo::setParameters(const QList<PayloadInfo::Parameter> &params) { d->parameters = params; }

bool PayloadInfo::operator==(const PayloadInfo &other) const { return (*d == *other.d); }

//----------------------------------------------------------------------------
// RtpSession
//----------------------------------------------------------------------------

RtpSession::RtpSession(QObject *parent) : QObject(parent) { d = new RtpSessionPrivate(this); }

RtpSession::~RtpSession() { delete d; }

void RtpSession::reset()
{
    delete d;
    d = new RtpSessionPrivate(this);
}

void RtpSession::setAudioOutputDevice(const QString &deviceId) { d->c->setAudioOutputDevice(deviceId); }

#ifdef QT_GUI_LIB
void RtpSession::setVideoOutputWidget(VideoWidget *widget) { d->c->setVideoOutputWidget(widget ? widget->d : nullptr); }
#endif

void RtpSession::setAudioInputDevice(const QString &deviceId) { d->c->setAudioInputDevice(deviceId); }

void RtpSession::setVideoInputDevice(const QString &deviceId) { d->c->setVideoInputDevice(deviceId); }

void RtpSession::setFileInput(const QString &fileName) { d->c->setFileInput(fileName); }

void RtpSession::setFileDataInput(const QByteArray &fileData) { d->c->setFileDataInput(fileData); }

void RtpSession::setFileLoopEnabled(bool enabled) { d->c->setFileLoopEnabled(enabled); }

#ifdef QT_GUI_LIB
void RtpSession::setVideoPreviewWidget(VideoWidget *widget)
{
    d->c->setVideoPreviewWidget(widget ? widget->d : nullptr);
}
#endif

void RtpSession::setRecordingQIODevice(QIODevice *dev) { d->c->setRecorder(dev); }

void RtpSession::stopRecording() { d->c->stopRecording(); }

void RtpSession::setLocalAudioPreferences(const QList<AudioParams> &params)
{
    QList<PAudioParams> list;
    foreach (const AudioParams &p, params)
        list += exportAudioParams(p);
    d->c->setLocalAudioPreferences(list);
}

void RtpSession::setLocalVideoPreferences(const QList<VideoParams> &params)
{
    QList<PVideoParams> list;
    foreach (const VideoParams &p, params)
        list += exportVideoParams(p);
    d->c->setLocalVideoPreferences(list);
}

void RtpSession::setMaximumSendingBitrate(int kbps) { d->c->setMaximumSendingBitrate(kbps); }

void RtpSession::setRemoteAudioPreferences(const QList<PayloadInfo> &info)
{
    QList<PPayloadInfo> list;
    foreach (const PayloadInfo &p, info)
        list += exportPayloadInfo(p);
    d->c->setRemoteAudioPreferences(list);
}

void RtpSession::setRemoteVideoPreferences(const QList<PayloadInfo> &info)
{
    QList<PPayloadInfo> list;
    foreach (const PayloadInfo &p, info)
        list += exportPayloadInfo(p);
    d->c->setRemoteVideoPreferences(list);
}

void RtpSession::start() { d->c->start(); }

void RtpSession::updatePreferences() { d->c->updatePreferences(); }

void RtpSession::transmitAudio() { d->c->transmitAudio(); }

void RtpSession::transmitVideo() { d->c->transmitVideo(); }

void RtpSession::pauseAudio() { d->c->pauseAudio(); }

void RtpSession::pauseVideo() { d->c->pauseVideo(); }

void RtpSession::stop() { d->c->stop(); }

QList<PayloadInfo> RtpSession::localAudioPayloadInfo() const
{
    QList<PayloadInfo> out;
    foreach (const PPayloadInfo &pp, d->c->localAudioPayloadInfo())
        out += importPayloadInfo(pp);
    return out;
}

QList<PayloadInfo> RtpSession::localVideoPayloadInfo() const
{
    QList<PayloadInfo> out;
    foreach (const PPayloadInfo &pp, d->c->localVideoPayloadInfo())
        out += importPayloadInfo(pp);
    return out;
}

QList<PayloadInfo> RtpSession::remoteAudioPayloadInfo() const
{
    QList<PayloadInfo> out;
    foreach (const PPayloadInfo &pp, d->c->remoteAudioPayloadInfo())
        out += importPayloadInfo(pp);
    return out;
}

QList<PayloadInfo> RtpSession::remoteVideoPayloadInfo() const
{
    QList<PayloadInfo> out;
    foreach (const PPayloadInfo &pp, d->c->remoteVideoPayloadInfo())
        out += importPayloadInfo(pp);
    return out;
}

QList<AudioParams> RtpSession::audioParams() const
{
    QList<AudioParams> out;
    foreach (const PAudioParams &pp, d->c->audioParams())
        out += importAudioParams(pp);
    return out;
}

QList<VideoParams> RtpSession::videoParams() const
{
    QList<VideoParams> out;
    foreach (const PVideoParams &pp, d->c->videoParams())
        out += importVideoParams(pp);
    return out;
}

bool RtpSession::canTransmitAudio() const { return d->c->canTransmitAudio(); }

bool RtpSession::canTransmitVideo() const { return d->c->canTransmitVideo(); }

int RtpSession::outputVolume() const { return d->c->outputVolume(); }

void RtpSession::setOutputVolume(int level) { d->c->setOutputVolume(level); }

int RtpSession::inputVolume() const { return d->c->inputVolume(); }

void RtpSession::setInputVolume(int level) { d->c->setInputVolume(level); }

RtpSession::Error RtpSession::errorCode() const { return RtpSession::Error(d->c->errorCode()); }

RtpChannel *RtpSession::audioRtpChannel() { return &d->audioRtpChannel; }

RtpChannel *RtpSession::videoRtpChannel() { return &d->videoRtpChannel; }
}; // namespace PsiMedia
