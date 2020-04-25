/*
 * Copyright (C) 2009  Barracuda Networks, Inc.
 *
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

#include "avcall.h"

#include "../psimedia/psimedia.h"
#include "applicationinfo.h"
#include "jingle-ice.h"
#include "jinglertp.h"
#include "mediadevicewatcher.h"
#include "psiaccount.h"
#include "psioptions.h"
#include "xmpp_client.h"
#include "xmpp_jid.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibrary>
#include <QtCrypto>
#include <stdio.h>
#include <stdlib.h>

#define USE_THREAD

// get default settings
static MediaConfiguration getDefaultConfiguration()
{
    MediaConfiguration config;
    config.liveInput = true;
    config.loopFile  = true;
    return config;
}

static MediaConfiguration *g_config = nullptr;

static void ensureConfig()
{
    if (!g_config) {
        g_config  = new MediaConfiguration;
        *g_config = getDefaultConfiguration();
    }
}

#ifdef GSTPROVIDER_STATIC
Q_IMPORT_PLUGIN(gstprovider)
#endif

#ifndef GSTPROVIDER_STATIC
static QString findPlugin(const QString &relpath, const QString &basename)
{
    QDir dir(QCoreApplication::applicationDirPath());
    if (!dir.cd(relpath))
        return QString();
    for (const QString &fileName : dir.entryList()) {
        if (fileName.contains(basename)) {
            QString filePath = dir.filePath(fileName);
            if (QLibrary::isLibrary(filePath))
                return filePath;
        }
    }
    return QString();
}
#endif

static bool g_loaded = false;

static void ensureLoaded()
{
    if (!g_loaded) {
#ifndef GSTPROVIDER_STATIC
        QString pluginFile;
        QString resourcePath;

        pluginFile = qgetenv("PSI_MEDIA_PLUGIN");
        if (pluginFile.isEmpty()) {
#if defined(Q_OS_WIN)
            pluginFile   = findPlugin(".", "gstprovider" DEBUG_POSTFIX);
            resourcePath = QCoreApplication::applicationDirPath() + "/gstreamer-0.10";
#elif defined(Q_OS_MAC)
            pluginFile   = findPlugin("../Plugins", "gstprovider" DEBUG_POSTFIX);
            resourcePath = QCoreApplication::applicationDirPath() + "/../Frameworks/gstreamer-0.10";
#else
            for (const QString &path : ApplicationInfo::pluginDirs()) {
                pluginFile = findPlugin(path, "gstprovider" DEBUG_POSTFIX);
                if (!pluginFile.isEmpty())
                    break;
            }
#endif
        }

        PsiMedia::PluginResult r = PsiMedia::loadPlugin(pluginFile, resourcePath);
        if (r == PsiMedia::PluginSuccess)
            g_loaded = true;
#else
        g_loaded = true;
#endif
        if (g_loaded)
            ensureConfig();
    }
}

static JingleRtpPayloadType payloadInfoToPayloadType(const PsiMedia::PayloadInfo &pi)
{
    JingleRtpPayloadType out;
    out.id        = pi.id();
    out.name      = pi.name();
    out.clockrate = pi.clockrate();
    out.channels  = pi.channels();
    out.ptime     = pi.ptime();
    out.maxptime  = pi.maxptime();
    for (const PsiMedia::PayloadInfo::Parameter &pip : pi.parameters()) {
        JingleRtpPayloadType::Parameter ptp;
        ptp.name  = pip.name;
        ptp.value = pip.value;
        out.parameters += ptp;
    }
    return out;
}

static PsiMedia::PayloadInfo payloadTypeToPayloadInfo(const JingleRtpPayloadType &pt)
{
    PsiMedia::PayloadInfo out;
    out.setId(pt.id);
    out.setName(pt.name);
    out.setClockrate(pt.clockrate);
    out.setChannels(pt.channels);
    out.setPtime(pt.ptime);
    out.setMaxptime(pt.maxptime);
    QList<PsiMedia::PayloadInfo::Parameter> list;
    for (const JingleRtpPayloadType::Parameter &ptp : pt.parameters) {
        PsiMedia::PayloadInfo::Parameter pip;
        pip.name  = ptp.name;
        pip.value = ptp.value;
        list += pip;
    }
    out.setParameters(list);
    return out;
}

class AvTransmit : public QObject {
    Q_OBJECT

public:
    PsiMedia::RtpChannel *audio, *video;
    JingleRtpChannel *    transport;

    AvTransmit(PsiMedia::RtpChannel *_audio, PsiMedia::RtpChannel *_video, JingleRtpChannel *_transport,
               QObject *parent = nullptr) :
        QObject(parent),
        audio(_audio), video(_video), transport(_transport)
    {
        if (audio) {
            audio->setParent(this);
            connect(audio, SIGNAL(readyRead()), SLOT(audio_readyRead()));
        }

        if (video) {
            video->setParent(this);
            connect(video, SIGNAL(readyRead()), SLOT(video_readyRead()));
        }

        transport->setParent(this);
        connect(transport, SIGNAL(readyRead()), SLOT(transport_readyRead()));
        connect(transport, SIGNAL(packetsWritten(int)), SLOT(transport_packetsWritten(int)));
    }

    ~AvTransmit()
    {
        if (audio)
            audio->setParent(nullptr);
        if (video)
            video->setParent(nullptr);
        transport->setParent(nullptr);
    }

private slots:
    void audio_readyRead()
    {
        while (audio->packetsAvailable() > 0) {
            PsiMedia::RtpPacket packet = audio->read();

            JingleRtp::RtpPacket jpacket;
            jpacket.type       = JingleRtp::Audio;
            jpacket.portOffset = packet.portOffset();
            jpacket.value      = packet.rawValue();

            transport->write(jpacket);
        }
    }

    void video_readyRead()
    {
        while (video->packetsAvailable() > 0) {
            PsiMedia::RtpPacket packet = video->read();

            JingleRtp::RtpPacket jpacket;
            jpacket.type       = JingleRtp::Video;
            jpacket.portOffset = packet.portOffset();
            jpacket.value      = packet.rawValue();

            transport->write(jpacket);
        }
    }

    void transport_readyRead()
    {
        while (transport->packetsAvailable()) {
            JingleRtp::RtpPacket jpacket = transport->read();

            if (jpacket.type == JingleRtp::Audio && audio) // FIXME why audio could null but we still receive packets?
                                                           // (the check was added to fix a crash)
                audio->write(PsiMedia::RtpPacket(jpacket.value, jpacket.portOffset));
            else if (jpacket.type == JingleRtp::Video && video) //  FIXME see above
                video->write(PsiMedia::RtpPacket(jpacket.value, jpacket.portOffset));
        }
    }

    void transport_packetsWritten(int count)
    {
        Q_UNUSED(count);

        // nothing
    }
};

class AvTransmitHandler : public QObject {
    Q_OBJECT

public:
    AvTransmit *avTransmit;
    QThread *   previousThread;

    AvTransmitHandler(QObject *parent = nullptr) : QObject(parent), avTransmit(nullptr), previousThread(nullptr) { }

    ~AvTransmitHandler()
    {
        if (avTransmit)
            releaseAvTransmit();
    }

    // NOTE: the handler never touches these variables except here
    //   and on destruction, so it's safe to call this function from
    //   another thread if you know what you're doing.
    void setAvTransmit(AvTransmit *_avTransmit)
    {
        avTransmit     = _avTransmit;
        previousThread = avTransmit->thread();
        avTransmit->moveToThread(thread());
    }

    void releaseAvTransmit()
    {
        Q_ASSERT(avTransmit);
        avTransmit->moveToThread(previousThread);
        avTransmit = nullptr;
    }
};

class AvTransmitThread : public QCA::SyncThread {
    Q_OBJECT

public:
    AvTransmitHandler *handler;

    AvTransmitThread(QObject *parent = nullptr) : QCA::SyncThread(parent), handler(nullptr) { }

    ~AvTransmitThread() { stop(); }

protected:
    virtual void atStart() { handler = new AvTransmitHandler; }

    virtual void atEnd() { delete handler; }
};

//----------------------------------------------------------------------------
// AvCall
//----------------------------------------------------------------------------
class AvCallManagerPrivate : public QObject {
    Q_OBJECT

public:
    AvCallManager *             q                    = nullptr;
    PsiAccount *                pa                   = nullptr;
    JingleRtpManager *          rtpManager           = nullptr;
    XMPP::Jingle::ICE::Manager *irisJingleICEManager = nullptr;
    QList<AvCall *>             sessions;
    QList<AvCall *>             pending;

    AvCallManagerPrivate(PsiAccount *_pa, AvCallManager *_q);
    ~AvCallManagerPrivate();

    void unlink(AvCall *call);

private slots:
    void rtp_incomingReady();
};

class AvCallPrivate : public QObject {
    Q_OBJECT

public:
    AvCall *              q;
    AvCallManagerPrivate *manager;
    bool                  incoming;
    JingleRtp *           sess;
    PsiMedia::RtpSession  rtp;
    XMPP::Jid             peer;
    AvCall::Mode          mode;
    int                   bitrate;
    bool                  allowVideo;
    QString               errorString;
    bool                  transmitAudio;
    bool                  transmitVideo;
    bool                  transmitting;
    AvTransmit *          avTransmit;
    AvTransmitThread *    avTransmitThread;

    AvCallPrivate(AvCall *_q) :
        QObject(_q), q(_q), manager(nullptr), sess(nullptr), transmitAudio(false), transmitVideo(false),
        transmitting(false), avTransmit(nullptr), avTransmitThread(nullptr)
    {
        allowVideo = AvCallManager::isVideoSupported();

        connect(&rtp, SIGNAL(started()), SLOT(rtp_started()));
        connect(&rtp, SIGNAL(preferencesUpdated()), SLOT(rtp_preferencesUpdated()));
        connect(&rtp, SIGNAL(stopped()), SLOT(rtp_stopped()));
        connect(&rtp, SIGNAL(error()), SLOT(rtp_error()));
    }

    ~AvCallPrivate()
    {
        rtp.disconnect(this);
        cleanup();
        unlink();
    }

    void unlink()
    {
        if (manager) {
            // note that the object remains active, just
            //   dissociated from the manager
            manager->unlink(q);
            manager = nullptr;
        }
    }

    void startOutgoing()
    {
        if (!manager)
            return;

        manager->rtpManager->setBasePort(g_config->basePort);
        manager->rtpManager->setExternalAddress(g_config->extHost);
        manager->irisJingleICEManager->setBasePort(g_config->basePort);
        manager->irisJingleICEManager->setExternalAddress(g_config->extHost);

        start_rtp();
    }

    bool initIncoming()
    {
        setup_sess();

        // JingleRtp guarantees there will be at least one of audio or video
        bool offeredAudio = false;
        bool offeredVideo = false;
        if (!sess->remoteAudioPayloadTypes().isEmpty())
            offeredAudio = true;
        if (allowVideo && !sess->remoteVideoPayloadTypes().isEmpty())
            offeredVideo = true;

        if (offeredAudio && offeredVideo)
            mode = AvCall::Both;
        else if (offeredAudio)
            mode = AvCall::Audio;
        else if (offeredVideo)
            mode = AvCall::Video;
        else {
            // this could happen if only video is offered but
            //   we don't allow it
            return false;
        }

        return true;
    }

    void accept()
    {
        if (!manager)
            return;

        manager->rtpManager->setBasePort(g_config->basePort);
        manager->rtpManager->setExternalAddress(g_config->extHost);
        manager->irisJingleICEManager->setBasePort(g_config->basePort);
        manager->irisJingleICEManager->setExternalAddress(g_config->extHost);

        // kick off the acceptance negotiation while simultaneously
        //   initializing the rtp engine.  note that session-accept
        //   won't actually get sent to the peer until we call
        //   localMediaUpdated()
        int types;
        if (mode == AvCall::Both)
            types = JingleRtp::Audio | JingleRtp::Video;
        else if (mode == AvCall::Audio)
            types = JingleRtp::Audio;
        else // Video
            types = JingleRtp::Video;

        sess->accept(types);
        start_rtp();
    }

    void reject()
    {
        if (sess)
            sess->reject();
        cleanup();
    }

private:
    static QString rtpSessionErrorToString(PsiMedia::RtpSession::Error e)
    {
        QString str;
        switch (e) {
        case PsiMedia::RtpSession::ErrorSystem:
            str = tr("System error");
            break;
        case PsiMedia::RtpSession::ErrorCodec:
            str = tr("Codec error");
            break;
        default: // generic
            str = tr("Generic error");
            break;
        }
        return str;
    }

    void cleanup()
    {
        // if we had a thread, this will move the object back
        delete avTransmitThread;
        avTransmitThread = nullptr;

        delete avTransmit;
        avTransmit = nullptr;

        rtp.reset();

        delete sess;
        sess = nullptr;
    }

    void start_rtp()
    {
        MediaConfiguration &config = *g_config;

        transmitAudio = false;
        transmitVideo = false;

        if (config.liveInput) {
            if (config.audioInDeviceId.isEmpty() && config.videoInDeviceId.isEmpty()) {
                errorString
                    = tr("Cannot call without selecting a device.  Do you have a microphone?  Check the Psi options.");
                cleanup();
                emit q->error();
                return;
            }

            if ((mode == AvCall::Audio || mode == AvCall::Both) && !config.audioInDeviceId.isEmpty()) {
                rtp.setAudioInputDevice(config.audioInDeviceId);
                transmitAudio = true;
            } else
                rtp.setAudioInputDevice(QString());

            if ((mode == AvCall::Video || mode == AvCall::Both) && !config.videoInDeviceId.isEmpty() && allowVideo) {
                rtp.setVideoInputDevice(config.videoInDeviceId);
                transmitVideo = true;
            } else
                rtp.setVideoInputDevice(QString());
        } else // non-live (file) input
        {
            rtp.setFileInput(config.file);
            rtp.setFileLoopEnabled(config.loopFile);

            // we just assume the file has both audio and video.
            //   if it doesn't, no big deal, it'll still work.
            //   update: after starting, we can correct these
            //   variables.
            transmitAudio = true;
            transmitVideo = true;
        }

        if (!config.audioOutDeviceId.isEmpty())
            rtp.setAudioOutputDevice(config.audioOutDeviceId);

        // media types are flagged by params, even if empty
        QList<PsiMedia::AudioParams> audioParamsList;
        if (transmitAudio)
            audioParamsList += PsiMedia::AudioParams();
        rtp.setLocalAudioPreferences(audioParamsList);

        QList<PsiMedia::VideoParams> videoParamsList;
        if (transmitVideo)
            videoParamsList += PsiMedia::VideoParams();
        rtp.setLocalVideoPreferences(videoParamsList);

        // for incoming sessions, we have the remote media info at
        //   the start, so use it
        if (incoming)
            setup_remote_media();

        if (bitrate != -1)
            rtp.setMaximumSendingBitrate(bitrate);

        transmitting = false;
        rtp.start();
    }

    void setup_sess()
    {
        connect(sess, SIGNAL(rejected()), SLOT(sess_rejected()));
        connect(sess, SIGNAL(error()), SLOT(sess_error()));
        connect(sess, SIGNAL(activated()), SLOT(sess_activated()));
        connect(sess, SIGNAL(remoteMediaUpdated()), SLOT(sess_remoteMediaUpdated()));
    }

    void setup_remote_media()
    {
        if (transmitAudio) {
            QList<JingleRtpPayloadType>  payloadTypes = sess->remoteAudioPayloadTypes();
            QList<PsiMedia::PayloadInfo> list;
            foreach (const JingleRtpPayloadType &pt, payloadTypes)
                list += payloadTypeToPayloadInfo(pt);
            rtp.setRemoteAudioPreferences(list);
        }

        if (transmitVideo) {
            QList<JingleRtpPayloadType>  payloadTypes = sess->remoteVideoPayloadTypes();
            QList<PsiMedia::PayloadInfo> list;
            foreach (const JingleRtpPayloadType &pt, payloadTypes)
                list += payloadTypeToPayloadInfo(pt);
            rtp.setRemoteVideoPreferences(list);
        }

        // FIXME: if the remote side doesn't support a media type,
        //   then we need to downgrade locally
    }

private slots:
    void rtp_started()
    {
        if (!manager)
            return;

        printf("rtp_started\n");

        if (!incoming) {
            sess = manager->rtpManager->createOutgoing();
            setup_sess();
        }

        if (transmitAudio && !rtp.localAudioPayloadInfo().isEmpty()) {
            QList<JingleRtpPayloadType> pis;
            for (PsiMedia::PayloadInfo pi : rtp.localAudioPayloadInfo()) {
                JingleRtpPayloadType pt = payloadInfoToPayloadType(pi);
                pis << pt;
            }
            sess->setLocalAudioPayloadTypes(pis);
        } else
            transmitAudio = false;

        if (transmitVideo && !rtp.localVideoPayloadInfo().isEmpty()) {
            QList<JingleRtpPayloadType> pis;
            for (PsiMedia::PayloadInfo pi : rtp.localVideoPayloadInfo()) {
                JingleRtpPayloadType pt = payloadInfoToPayloadType(pi);
                pis << pt;
            }
            sess->setLocalVideoPayloadTypes(pis);
        } else
            transmitVideo = false;

        if (transmitAudio && transmitVideo)
            mode = AvCall::Both;
        else if (transmitAudio && !transmitVideo)
            mode = AvCall::Audio;
        else if (transmitVideo && !transmitAudio)
            mode = AvCall::Video;
        else {
            // can't happen?
            Q_ASSERT(0);
        }

        if (!incoming)
            sess->connectToJid(peer);
        else
            sess->localMediaUpdate();
    }

    void rtp_preferencesUpdated()
    {
        // nothing?
    }

    void rtp_stopped()
    {
        // nothing for now, until we do async shutdown
    }

    void rtp_error()
    {
        errorString = tr("An error occurred while trying to send:\n%1.").arg(rtpSessionErrorToString(rtp.errorCode()));
        reject();
        emit q->error();
    }

    void sess_rejected()
    {
        errorString = tr("Call was rejected or terminated.");
        cleanup();
        emit q->error();
    }

    void sess_error()
    {
        JingleRtp::Error e = sess->errorCode();
        if (e == JingleRtp::ErrorTimeout) {
            errorString = tr("Call negotiation timed out.");
            cleanup();
        } else if (e == JingleRtp::ErrorICE) {
            errorString = tr("Unable to establish peer-to-peer connection.");
            reject();
        } else {
            errorString = tr("Call negotiation failed.");
            cleanup();
        }

        emit q->error();
    }

    void sess_activated()
    {
        PsiMedia::RtpChannel *audio = nullptr;
        PsiMedia::RtpChannel *video = nullptr;

        if (transmitAudio)
            audio = rtp.audioRtpChannel();
        if (transmitVideo)
            video = rtp.videoRtpChannel();

        avTransmit = new AvTransmit(audio, video, sess->rtpChannel());
#ifdef USE_THREAD
        avTransmitThread = new AvTransmitThread(this);
        avTransmitThread->start();
        avTransmitThread->handler->setAvTransmit(avTransmit);
#endif

        if (transmitAudio)
            rtp.transmitAudio();
        if (transmitVideo)
            rtp.transmitVideo();

        transmitting = true;
        emit q->activated();
    }

    void sess_remoteMediaUpdated()
    {
        setup_remote_media();
        rtp.updatePreferences();
    }
};

AvCall::AvCall() { d = new AvCallPrivate(this); }

AvCall::AvCall(const AvCall &from) : QObject(nullptr)
{
    Q_UNUSED(from);
    fprintf(stderr, "AvCall copy not supported\n");
    abort();
}

AvCall::~AvCall() { delete d; }

XMPP::Jid AvCall::jid() const
{
    if (d->sess)
        return d->sess->jid();
    else
        return XMPP::Jid();
}

AvCall::Mode AvCall::mode() const { return d->mode; }

void AvCall::connectToJid(const XMPP::Jid &jid, Mode mode, int kbps)
{
    d->peer    = jid;
    d->mode    = mode;
    d->bitrate = kbps;
    d->startOutgoing();
}

void AvCall::accept(Mode mode, int kbps)
{
    d->mode    = mode;
    d->bitrate = kbps;
    d->accept();
}

void AvCall::reject() { d->reject(); }

void AvCall::setIncomingVideo(PsiMedia::VideoWidget *widget) { d->rtp.setVideoOutputWidget(widget); }

QString AvCall::errorString() const { return d->errorString; }

void AvCall::unlink() { d->unlink(); }

//----------------------------------------------------------------------------
// AvCallManager
//----------------------------------------------------------------------------
AvCallManagerPrivate::AvCallManagerPrivate(PsiAccount *_pa, AvCallManager *_q) : QObject(_q), q(_q), pa(_pa)
{
    rtpManager           = new JingleRtpManager(pa->client());
    irisJingleICEManager = pa->client()->jingleICEManager();
    connect(rtpManager, SIGNAL(incomingReady()), SLOT(rtp_incomingReady()));
}

AvCallManagerPrivate::~AvCallManagerPrivate() { delete rtpManager; }

void AvCallManagerPrivate::unlink(AvCall *call) { sessions.removeAll(call); }

void AvCallManagerPrivate::rtp_incomingReady()
{
    AvCall *call      = new AvCall;
    call->d->manager  = this;
    call->d->incoming = true;
    call->d->sess     = rtpManager->takeIncoming();
    sessions += call;
    if (!call->d->initIncoming()) {
        call->d->sess->reject();
        delete call->d->sess;
        call->d->sess = nullptr;
        delete call;
        return;
    }

    pending += call;
    emit q->incomingReady();
}

AvCallManager::AvCallManager(PsiAccount *pa) : QObject(nullptr) { d = new AvCallManagerPrivate(pa, this); }

AvCallManager::~AvCallManager() { delete d; }

AvCall *AvCallManager::createOutgoing()
{
    AvCall *call      = new AvCall;
    call->d->manager  = d;
    call->d->incoming = false;
    return call;
}

AvCall *AvCallManager::takeIncoming() { return d->pending.takeFirst(); }

void AvCallManager::config()
{
    // TODO: remove this function?
}

bool AvCallManager::isSupported()
{
    ensureLoaded();
    if (!QCA::isSupported("hmac(sha1)")) {
        printf("hmac support missing for voice calls, install qca-ossl\n");
        return false;
    }
    return PsiMedia::isSupported();
}

bool AvCallManager::isVideoSupported()
{
    if (!isSupported())
        return false;

    if (PsiOptions::instance()->getOption("options.media.video-support").toBool())
        return true;

    if (!QString::fromLatin1(qgetenv("PSI_ENABLE_VIDEO")).isEmpty())
        return true;
    else
        return false;
}

void AvCallManager::setSelfAddress(const QHostAddress &addr)
{
    d->rtpManager->setSelfAddress(addr);
    d->irisJingleICEManager->setSelfAddress(addr);
}

void AvCallManager::setStunBindService(const QString &host, int port)
{
    d->rtpManager->setStunBindService(host, port);
    d->irisJingleICEManager->setStunBindService(host, port);
}

void AvCallManager::setStunRelayUdpService(const QString &host, int port, const QString &user, const QString &pass)
{
    d->rtpManager->setStunRelayUdpService(host, port, user, pass);
    d->irisJingleICEManager->setStunRelayUdpService(host, port, user, pass);
}

void AvCallManager::setStunRelayTcpService(const QString &host, int port, const XMPP::AdvancedConnector::Proxy &proxy,
                                           const QString &user, const QString &pass)
{
    d->rtpManager->setStunRelayTcpService(host, port, proxy, user, pass);
    d->irisJingleICEManager->setStunRelayTcpService(host, port, proxy, user, pass);
}

void AvCallManager::setBasePort(int port)
{
    if (port == 0)
        port = -1;
    g_config->basePort = port;
}

void AvCallManager::setExternalAddress(const QString &host) { g_config->extHost = host; }

void AvCallManager::setAudioOutDevice(const QString &id) { g_config->audioOutDeviceId = id; }

void AvCallManager::setAudioInDevice(const QString &id) { g_config->audioInDeviceId = id; }

void AvCallManager::setVideoInDevice(const QString &id) { g_config->videoInDeviceId = id; }

#include "avcall.moc"
