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

#include "jinglertp.h"

#include "iceagent.h"
#include "iris/iceagent.h"
#include "iris/netnames.h"
#include "iris/turnclient.h"
#include "iris/udpportreserver.h"
#include "jingle.h"
#include "xmpp_client.h"

#include <QtCrypto>
#include <stdio.h>
#include <stdlib.h>

// TODO: reject offers that don't contain at least one of audio or video
// TODO: support candidate negotiations over the JingleRtpChannel thread
//   boundary, so we can change candidates after the stream is active

// scope values: 0 = local, 1 = link-local, 2 = private, 3 = public
static int getAddressScope(const QHostAddress &a)
{
    if (a.protocol() == QAbstractSocket::IPv6Protocol) {
        if (a == QHostAddress(QHostAddress::LocalHostIPv6))
            return 0;
        else if (XMPP::Ice176::isIPv6LinkLocalAddress(a))
            return 1;
    } else if (a.protocol() == QAbstractSocket::IPv4Protocol) {
        quint32 v4 = a.toIPv4Address();
        quint8  a0 = v4 >> 24;
        quint8  a1 = (v4 >> 16) & 0xff;
        if (a0 == 127)
            return 0;
        else if (a0 == 169 && a1 == 254)
            return 1;
        else if (a0 == 10)
            return 2;
        else if (a0 == 172 && a1 >= 16 && a1 <= 31)
            return 2;
        else if (a0 == 192 && a1 == 168)
            return 2;
    }

    return 3;
}

// -1 = a is higher priority, 1 = b is higher priority, 0 = equal
static int comparePriority(const QHostAddress &a, const QHostAddress &b)
{
    // prefer closer scope
    int a_scope = getAddressScope(a);
    int b_scope = getAddressScope(b);
    if (a_scope < b_scope)
        return -1;
    else if (a_scope > b_scope)
        return 1;

    // prefer ipv6
    if (a.protocol() == QAbstractSocket::IPv6Protocol && b.protocol() != QAbstractSocket::IPv6Protocol)
        return -1;
    else if (b.protocol() == QAbstractSocket::IPv6Protocol && a.protocol() != QAbstractSocket::IPv6Protocol)
        return 1;

    return 0;
}

static QList<QHostAddress> sortAddrs(const QList<QHostAddress> &in)
{
    QList<QHostAddress> out;

    for (const QHostAddress &a : in) {
        int at;
        for (at = 0; at < out.count(); ++at) {
            if (comparePriority(a, out[at]) < 0)
                break;
        }

        out.insert(at, a);
    }

    return out;
}

// resolve external address and stun server
// TODO: resolve hosts and start ice engine simultaneously
// FIXME: when/if our ICE engine supports adding these dynamically, we should
//   not have the lookups block on each other
class Resolver : public QObject {
    Q_OBJECT

private:
    XMPP::NameResolver dnsA;
    XMPP::NameResolver dnsB;
    XMPP::NameResolver dnsC;
    XMPP::NameResolver dnsD;
    QString            extHost;
    QString            stunBindHost, stunRelayUdpHost, stunRelayTcpHost;
    bool               extDone;
    bool               stunBindDone;
    bool               stunRelayUdpDone;
    bool               stunRelayTcpDone;

public:
    QHostAddress extAddr;
    QHostAddress stunBindAddr, stunRelayUdpAddr, stunRelayTcpAddr;

    Resolver(QObject *parent = nullptr) : QObject(parent), dnsA(parent), dnsB(parent), dnsC(parent), dnsD(parent)
    {
        connect(&dnsA, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)),
                SLOT(dns_resultsReady(const QList<XMPP::NameRecord> &)));
        connect(&dnsA, SIGNAL(error(XMPP::NameResolver::Error)), SLOT(dns_error(XMPP::NameResolver::Error)));

        connect(&dnsB, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)),
                SLOT(dns_resultsReady(const QList<XMPP::NameRecord> &)));
        connect(&dnsB, SIGNAL(error(XMPP::NameResolver::Error)), SLOT(dns_error(XMPP::NameResolver::Error)));

        connect(&dnsC, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)),
                SLOT(dns_resultsReady(const QList<XMPP::NameRecord> &)));
        connect(&dnsC, SIGNAL(error(XMPP::NameResolver::Error)), SLOT(dns_error(XMPP::NameResolver::Error)));

        connect(&dnsD, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)),
                SLOT(dns_resultsReady(const QList<XMPP::NameRecord> &)));
        connect(&dnsD, SIGNAL(error(XMPP::NameResolver::Error)), SLOT(dns_error(XMPP::NameResolver::Error)));
    }

    void start(const QString &_extHost, const QString &_stunBindHost, const QString &_stunRelayUdpHost,
               const QString &_stunRelayTcpHost)
    {
        extHost          = _extHost;
        stunBindHost     = _stunBindHost;
        stunRelayUdpHost = _stunRelayUdpHost;
        stunRelayTcpHost = _stunRelayTcpHost;

        if (!extHost.isEmpty()) {
            extDone = false;
            dnsA.start(extHost.toLatin1());
        } else
            extDone = true;

        if (!stunBindHost.isEmpty()) {
            stunBindDone = false;
            dnsB.start(stunBindHost.toLatin1());
        } else
            stunBindDone = true;

        if (!stunRelayUdpHost.isEmpty()) {
            stunRelayUdpDone = false;
            dnsC.start(stunRelayUdpHost.toLatin1());
        } else
            stunRelayUdpDone = true;

        if (!stunRelayTcpHost.isEmpty()) {
            stunRelayTcpDone = false;
            dnsD.start(stunRelayTcpHost.toLatin1());
        } else
            stunRelayTcpDone = true;

        if (extDone && stunBindDone && stunRelayUdpDone && stunRelayTcpDone)
            QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
    }

signals:
    void finished();

private slots:
    void dns_resultsReady(const QList<XMPP::NameRecord> &results)
    {
        XMPP::NameResolver *dns = static_cast<XMPP::NameResolver *>(sender());

        // FIXME: support more than one address?
        QHostAddress addr = results.first().address();

        if (dns == &dnsA) {
            extAddr = addr;
            extDone = true;
            tryFinish();
        } else if (dns == &dnsB) {
            stunBindAddr = addr;
            stunBindDone = true;
            tryFinish();
        } else if (dns == &dnsC) {
            stunRelayUdpAddr = addr;
            stunRelayUdpDone = true;
            tryFinish();
        } else // dnsD
        {
            stunRelayTcpAddr = addr;
            stunRelayTcpDone = true;
            tryFinish();
        }
    }

    void dns_error(XMPP::NameResolver::Error e)
    {
        Q_UNUSED(e);

        XMPP::NameResolver *dns = static_cast<XMPP::NameResolver *>(sender());

        if (dns == &dnsA) {
            extDone = true;
            tryFinish();
        } else if (dns == &dnsB) {
            stunBindDone = true;
            tryFinish();
        } else if (dns == &dnsC) {
            stunRelayUdpDone = true;
            tryFinish();
        } else // dnsD
        {
            stunRelayTcpDone = true;
            tryFinish();
        }
    }

private:
    void tryFinish()
    {
        if (extDone && stunBindDone && stunRelayUdpDone && stunRelayTcpDone)
            emit finished();
    }
};

class IceStopper : public QObject {
    Q_OBJECT

public:
    QTimer                 t;
    XMPP::UdpPortReserver *portReserver;
    QList<XMPP::Ice176 *>  left;

    IceStopper(QObject *parent = nullptr) : QObject(parent), t(this), portReserver(nullptr)
    {
        connect(&t, &QTimer::timeout, this, &IceStopper::deleteLater);
        t.setSingleShot(true);
    }

    ~IceStopper()
    {
        qDeleteAll(left);
        delete portReserver;
        printf("IceStopper done\n");
    }

    void start(XMPP::UdpPortReserver *_portReserver, QList<XMPP::Ice176 *> &&iceList)
    {
        if (_portReserver) {
            portReserver = _portReserver;
            portReserver->setParent(this);
        }
        left = iceList;

        for (XMPP::Ice176 *ice : left) {
            ice->setParent(this);
            if (ice->isStopped()) {
                ice_stopped(ice);
                continue;
            }

            // TODO: error() also?
            connect(ice, &XMPP::Ice176::stopped, this, [this, ice]() { ice_stopped(ice); });
            connect(ice, &XMPP::Ice176::error, this, [this, ice](XMPP::Ice176::Error) { ice_stopped(ice); });
            ice->stop();
        }

        t.start(3000);
    }

private:
    void ice_stopped(XMPP::Ice176 *ice)
    {
        ice->disconnect(this);
        ice->setParent(nullptr);
        ice->deleteLater();
        left.removeAll(ice);
        if (left.isEmpty())
            deleteLater();
    }
};

//----------------------------------------------------------------------------
// JingleRtp
//----------------------------------------------------------------------------
class JingleRtpChannelPrivate : public QObject {
    Q_OBJECT

public:
    JingleRtpChannel *q;

    QMutex                      m;
    XMPP::UdpPortReserver *     portReserver;
    XMPP::Ice176 *              iceA;
    XMPP::Ice176 *              iceV;
    QTimer *                    rtpActivityTimer;
    QList<JingleRtp::RtpPacket> in;

    JingleRtpChannelPrivate(JingleRtpChannel *_q);
    ~JingleRtpChannelPrivate();

    void setIceObjects(XMPP::UdpPortReserver *_portReserver, XMPP::Ice176 *_iceA, XMPP::Ice176 *_iceV);
    void restartRtpActivityTimer();

private slots:
    void start();
    void ice_readyRead(int componentIndex);
    void ice_datagramsWritten(int componentIndex, int count);
    void rtpActivity_timeout();
};

class JingleRtpManagerPrivate : public QObject {
    Q_OBJECT

public:
    JingleRtpManager *q;

    XMPP::Client *          client;
    QHostAddress            selfAddr;
    QString                 extHost;
    QString                 stunBindHost;
    int                     stunBindPort;
    QString                 stunRelayUdpHost;
    int                     stunRelayUdpPort;
    QString                 stunRelayUdpUser;
    QString                 stunRelayUdpPass;
    QString                 stunRelayTcpHost;
    int                     stunRelayTcpPort;
    QString                 stunRelayTcpUser;
    QString                 stunRelayTcpPass;
    bool                    allowIpExposure = true;
    XMPP::TurnClient::Proxy stunProxy;
    int                     basePort;
    QList<JingleRtp *>      sessions;
    QList<JingleRtp *>      pending;
    JT_PushJingleRtp *      push_task;

    JingleRtpManagerPrivate(XMPP::Client *_client, JingleRtpManager *_q);
    ~JingleRtpManagerPrivate();

    QString createSid(const XMPP::Jid &peer) const;

    void unlink(JingleRtp *sess);

private slots:
    void push_task_incomingRequest(const XMPP::Jid &from, const QString &iq_id, const JingleRtpEnvelope &envelope);
};

class JingleRtpPrivate : public QObject {
    Q_OBJECT

public:
    JingleRtp *q;

    JingleRtpManagerPrivate *manager  = nullptr;
    bool                     incoming = false;
    XMPP::Jid                peer;
    JingleRtp::PeerFeatures  peerFeatures;
    QString                  sid;

    int                         types = 0;
    QList<JingleRtpPayloadType> localAudioPayloadTypes;
    QList<JingleRtpPayloadType> localVideoPayloadTypes;
    QList<JingleRtpPayloadType> remoteAudioPayloadTypes;
    QList<JingleRtpPayloadType> remoteVideoPayloadTypes;
    int                         localMaximumBitrate  = -1;
    int                         remoteMaximumBitrate = -1;
    QString                     audioName;
    QString                     videoName;

    QString                init_iq_id;
    QPointer<JT_JingleRtp> jt;

    Resolver          resolver;
    QHostAddress      extAddr;
    QHostAddress      stunBindAddr, stunRelayUdpAddr, stunRelayTcpAddr;
    int               stunBindPort = 0, stunRelayUdpPort = 0, stunRelayTcpPort = 0;
    XMPP::Ice176 *    iceA       = nullptr;
    XMPP::Ice176 *    iceV       = nullptr;
    JingleRtpChannel *rtpChannel = nullptr;

    class IceStatus {
    public:
        bool started                       = false;
        bool ice2                          = false;
        bool notifyLocalGatheringComplete  = false;
        bool notifyRemoteGatheringComplete = false;

        JingleRtpTrans::Transport transportType = JingleRtpTrans::IceUdp;

        QString remoteUfrag;
        QString remotePassword;

        // for queuing up candidates before using them
        QList<XMPP::Ice176::Candidate> localCandidates;
        QList<XMPP::Ice176::Candidate> remoteCandidates;
    };
    IceStatus iceA_status;
    IceStatus iceV_status;

    bool local_media_ready   = false;
    bool prov_accepted       = false; // remote knows of the session
    bool ice_started         = false; // for all streams
    bool ice_connected       = false; // for all streams
    bool session_accept_sent = false;
    bool session_activated   = false;

    JingleRtp::Error       errorCode    = JingleRtp::NoError;
    XMPP::UdpPortReserver *portReserver = nullptr;

    JingleRtpPrivate(JingleRtp *_q) : QObject(_q), q(_q)
    {
        connect(&resolver, SIGNAL(finished()), SLOT(resolver_finished()));
        rtpChannel = new JingleRtpChannel;
    }

    ~JingleRtpPrivate()
    {
        cleanup();
        manager->unlink(q);
        delete rtpChannel;
    }

    void startOutgoing()
    {
        local_media_ready = true;

        types = 0;
        if (!localAudioPayloadTypes.isEmpty()) {
            printf("there are audio payload types\n");
            types |= JingleRtp::Audio;
            iceA_status.transportType
                = (peerFeatures & JingleRtp::IceTransport) ? JingleRtpTrans::Ice : JingleRtpTrans::IceUdp;
            if (iceA_status.transportType == JingleRtpTrans::Ice)
                iceA_status.ice2 = true; // try ice2 by default
        }
        if (!localVideoPayloadTypes.isEmpty()) {
            printf("there are video payload types\n");
            types |= JingleRtp::Video;
            iceV_status.transportType
                = (peerFeatures & JingleRtp::IceTransport) ? JingleRtpTrans::Ice : JingleRtpTrans::IceUdp;
            if (iceV_status.transportType == JingleRtpTrans::Ice)
                iceV_status.ice2 = true; // try ice2 by default
        }

        printf("types=%d\n", types);
        resolver.start(manager->extHost, manager->stunBindHost, manager->stunRelayUdpHost, manager->stunRelayTcpHost);
    }

    void accept(int _types)
    {
        types = _types;

        // TODO: cancel away whatever media type is not used

        resolver.start(manager->extHost, manager->stunBindHost, manager->stunRelayUdpHost, manager->stunRelayTcpHost);
    }

    void reject()
    {
        if (incoming) {
            // send iq-result if we haven't done so yet
            if (!prov_accepted) {
                prov_accepted = true;
                manager->push_task->respondSuccess(peer, init_iq_id);
            }
        } else {
            bool ok = true;
            if ((types & JingleRtp::Audio) && !iceA_status.started)
                ok = false;
            if ((types & JingleRtp::Video) && !iceV_status.started)
                ok = false;

            // we haven't even sent session-initiate
            if (!ok) {
                cleanup();
                return;
            }
        }

        JingleRtpEnvelope envelope;
        envelope.action           = "session-terminate";
        envelope.sid              = sid;
        envelope.reason.condition = JingleRtpReason::Gone;

        JT_JingleRtp *task = new JT_JingleRtp(manager->client->rootTask());
        task->request(peer, envelope);
        task->go(true);

        cleanup();
    }

    void localMediaUpdate()
    {
        local_media_ready = true;

        tryAccept();
        tryActivated();
    }

    // called by manager when request is received, including
    //   session-initiate.
    // note: manager will never send session-initiate twice.
    bool incomingRequest(const QString &iq_id, const JingleRtpEnvelope &envelope)
    {
        // TODO: jingle has a lot of fields, and we kind of skip over
        //   most of them just to grab what we need.  perhaps in the
        //   future we could do more integrity checking.

        if (envelope.action == "session-initiate") {
            // initially flag both types, so we don't drop any
            //   transport-info before we accept (at which point
            //   we specify what types we actually want)
            types = JingleRtp::Audio | JingleRtp::Video;

            init_iq_id = iq_id;

            const JingleRtpContent *audioContent = nullptr;
            const JingleRtpContent *videoContent = nullptr;

            // find content
            for (const JingleRtpContent &c : envelope.contentList) {
                if ((types & JingleRtp::Audio) && c.desc.media == "audio" && !audioContent) {
                    audioContent = &c;
                } else if ((types & JingleRtp::Video) && c.desc.media == "video" && !videoContent) {
                    videoContent = &c;
                }
            }

            if (audioContent) {
                audioName               = audioContent->name;
                remoteAudioPayloadTypes = audioContent->desc.payloadTypes;
                if (!audioContent->trans.user.isEmpty()) {
                    iceA_status.remoteUfrag    = audioContent->trans.user;
                    iceA_status.remotePassword = audioContent->trans.pass;
                }
                iceA_status.ice2          = audioContent->trans.ice2;
                iceA_status.transportType = audioContent->trans.transportType;

                iceA_status.remoteCandidates += audioContent->trans.candidates;
            }
            if (videoContent) {
                videoName               = videoContent->name;
                remoteVideoPayloadTypes = videoContent->desc.payloadTypes;
                if (!videoContent->trans.user.isEmpty()) {
                    iceV_status.remoteUfrag    = videoContent->trans.user;
                    iceV_status.remotePassword = videoContent->trans.pass;
                }
                iceV_status.ice2          = videoContent->trans.ice2;
                iceV_status.transportType = videoContent->trans.transportType;

                iceV_status.remoteCandidates += videoContent->trans.candidates;
            }

            // must offer at least one audio or video payload
            if (remoteAudioPayloadTypes.isEmpty() && remoteVideoPayloadTypes.isEmpty())
                return false; // caller will respond with 400. see JingleRtpManagerPrivate::push_task_incomingRequest

            prov_accepted = true; // caller will respond with Ok immediatelly
        } else if (envelope.action == "session-accept" && !incoming) {
            manager->push_task->respondSuccess(peer, iq_id);

            const JingleRtpContent *audioContent = nullptr;
            const JingleRtpContent *videoContent = nullptr;

            // find content
            for (const JingleRtpContent &c : envelope.contentList) {
                if ((types & JingleRtp::Audio) && c.desc.media == "audio" && c.name == audioName
                    && c.trans.transportType == iceA_status.transportType && !audioContent) {
                    audioContent = &c;
                } else if ((types & JingleRtp::Video) && c.desc.media == "video" && c.name == videoName
                           && c.trans.transportType == iceV_status.transportType && !videoContent) {
                    videoContent = &c;
                }
            }

            // we support audio, peer doesn't
            if ((types & JingleRtp::Audio) && !audioContent)
                types &= ~JingleRtp::Audio;

            // we support video, peer doesn't
            if ((types & JingleRtp::Video) && !videoContent)
                types &= ~JingleRtp::Video;

            if (types == 0) {
                qDebug("None of requested content was accepted! Cleaning up..");
                reject();
                emit q->rejected();
                return false;
            }

            if (audioContent) {
                remoteAudioPayloadTypes = audioContent->desc.payloadTypes;
                if (!audioContent->trans.user.isEmpty()) {
                    iceA_status.remoteUfrag    = audioContent->trans.user;
                    iceA_status.remotePassword = audioContent->trans.pass;
                }
                iceA_status.remoteCandidates += audioContent->trans.candidates;
                iceA_status.notifyRemoteGatheringComplete = audioContent->trans.gatheringComplete;
                iceA_status.ice2                          = audioContent->trans.ice2;
                iceA->setRemoteFeatures(evalIceFeatures(iceA_status));
                iceA->startChecks();
            } else if (iceA) {
                qDebug("Audio was requested but not accepted! Cleaning up..");
                auto is = new IceStopper();
                is->start(portReserver, { iceA });
                iceA = nullptr;
            }
            if (videoContent) {
                remoteVideoPayloadTypes = videoContent->desc.payloadTypes;
                if (!videoContent->trans.user.isEmpty()) {
                    iceV_status.remoteUfrag    = videoContent->trans.user;
                    iceV_status.remotePassword = videoContent->trans.pass;
                }
                iceV_status.remoteCandidates += videoContent->trans.candidates;
                iceV_status.notifyRemoteGatheringComplete = videoContent->trans.gatheringComplete;
                iceV_status.ice2                          = videoContent->trans.ice2;
                iceV->setRemoteFeatures(evalIceFeatures(iceV_status));
                iceV->startChecks();
            } else if (iceV) {
                qDebug("Video was requested but not accepted! Cleaning up..");
                auto is = new IceStopper();
                is->start(portReserver, { iceV });
                iceV = nullptr;
            }

            flushRemoteCandidates();

            session_accept_sent = true;
            QMetaObject::invokeMethod(this, "after_session_accept", Qt::QueuedConnection);
            emit q->remoteMediaUpdated();
        } else if (envelope.action == "session-terminate") {
            manager->push_task->respondSuccess(peer, iq_id);

            cleanup();
            emit q->rejected();
        } else if (envelope.action == "transport-info") {
            manager->push_task->respondSuccess(peer, iq_id);

            const JingleRtpContent *audioContent = nullptr;
            const JingleRtpContent *videoContent = nullptr;

            // find content
            for (const JingleRtpContent &c : envelope.contentList) {
                if ((types & JingleRtp::Audio) && c.name == audioName && !audioContent) {
                    audioContent = &c;
                } else if ((types & JingleRtp::Video) && c.name == videoName && !videoContent) {
                    videoContent = &c;
                }
            }

            if (audioContent) {
                if (!audioContent->trans.user.isEmpty()) {
                    iceA_status.remoteUfrag    = audioContent->trans.user;
                    iceA_status.remotePassword = audioContent->trans.pass;
                }

                printf("audio candidates=%d\n", audioContent->trans.candidates.count());
                iceA_status.remoteCandidates += audioContent->trans.candidates;
            }
            if (videoContent) {
                if (!videoContent->trans.user.isEmpty()) {
                    iceV_status.remoteUfrag    = videoContent->trans.user;
                    iceV_status.remotePassword = videoContent->trans.pass;
                }

                printf("video candidates=%d\n", videoContent->trans.candidates.count());
                iceV_status.remoteCandidates += videoContent->trans.candidates;
            }

            // don't process the candidates unless our ICE engine
            //   is started
            if (prov_accepted)
                flushRemoteCandidates();
        } else {
            manager->push_task->respondError(peer, iq_id, 400, QString());
            return false;
        }

        return true;
    }

private:
    void cleanup()
    {
        resolver.disconnect(this);

        if (jt) {
            jt->disconnect(this);
            jt->deleteLater();
            jt = nullptr;
        }

        if (portReserver) {
            portReserver->setParent(nullptr);

            QList<XMPP::Ice176 *> list;

            if (iceA) {
                iceA->disconnect(this);
                iceA->setParent(nullptr);
                list += iceA;
                iceA = nullptr;
            }

            if (iceV) {
                iceV->disconnect(this);
                iceV->setParent(nullptr);
                list += iceV;
                iceV = nullptr;
            }

            // pass ownership of portReserver, iceA, and iceV
            IceStopper *iceStopper = new IceStopper;
            iceStopper->start(portReserver, std::move(list));

            portReserver = nullptr;
        } else {
            if (iceA) {
                iceA->disconnect(this);
                iceA->setParent(nullptr);
                iceA->deleteLater();
                iceA = nullptr;
            }

            if (iceV) {
                iceV->disconnect(this);
                iceV->setParent(nullptr);
                iceV->deleteLater();
                iceV = nullptr;
            }

            delete portReserver;
            portReserver = nullptr;
        }

        // prevent delivery of events by manager
        peer = XMPP::Jid();
        sid.clear();
    }

    XMPP::Ice176::Features evalIceFeatures(const IceStatus &s)
    {
        constexpr auto iceUdpOpts
            = XMPP::Ice176::Trickle | XMPP::Ice176::AggressiveNomination | XMPP::Ice176::RTPOptimization;
        constexpr auto ice1opts = XMPP::Ice176::Trickle | XMPP::Ice176::AggressiveNomination
            | XMPP::Ice176::RTPOptimization | XMPP::Ice176::GatheringComplete;
        constexpr auto ice2opts
            = XMPP::Ice176::Trickle | XMPP::Ice176::NotNominatedData | XMPP::Ice176::GatheringComplete;

        return s.transportType == JingleRtpTrans::IceUdp ? iceUdpOpts : (s.ice2 ? ice2opts : ice1opts);
    }

    void start_ice()
    {
        stunBindPort     = manager->stunBindPort;
        stunRelayUdpPort = manager->stunRelayUdpPort;
        stunRelayTcpPort = manager->stunRelayTcpPort;
        if (!stunBindAddr.isNull() && stunBindPort > 0)
            printf("STUN service: %s;%d\n", qPrintable(stunBindAddr.toString()), stunBindPort);
        if (!stunRelayUdpAddr.isNull() && stunRelayUdpPort > 0 && !manager->stunRelayUdpUser.isEmpty())
            printf("TURN w/ UDP service: %s;%d\n", qPrintable(stunRelayUdpAddr.toString()), stunRelayUdpPort);
        if (!stunRelayTcpAddr.isNull() && stunRelayTcpPort > 0 && !manager->stunRelayTcpUser.isEmpty())
            printf("TURN w/ TCP service: %s;%d\n", qPrintable(stunRelayTcpAddr.toString()), stunRelayTcpPort);

        QList<QHostAddress> listenAddrs;
        auto const          interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &ni : interfaces) {
            const auto entries = ni.addressEntries();
            for (const QNetworkAddressEntry &na : entries) {
                QHostAddress h = na.ip();

                // skip localhost
                if (getAddressScope(h) == 0)
                    continue;

                // don't put the same address in twice.
                //   this also means that if there are
                //   two link-local ipv6 interfaces
                //   with the exact same address, we
                //   only use the first one
                if (listenAddrs.contains(h))
                    continue;

                if (h.protocol() == QAbstractSocket::IPv6Protocol && XMPP::Ice176::isIPv6LinkLocalAddress(h))
                    h.setScopeId(ni.name());
                listenAddrs += h;
            }
        }

        listenAddrs = sortAddrs(listenAddrs);

        QList<XMPP::Ice176::LocalAddress> localAddrs;

        QStringList strList;
        for (const auto &h : listenAddrs) {
            localAddrs += XMPP::Ice176::LocalAddress { h };
            strList += h.toString();
        }

        if (manager->basePort != -1) {
            portReserver = new XMPP::UdpPortReserver(this);
            portReserver->setAddresses(listenAddrs);
            portReserver->setPorts(manager->basePort, 4);
        }

        if (!strList.isEmpty()) {
            printf("Host addresses:\n");
            for (const QString &s : strList)
                printf("  %s\n", qPrintable(s));
        }

        if (types & JingleRtp::Audio) {
            iceA = new XMPP::Ice176(this);
            iceA->setLocalFeatures(evalIceFeatures(iceA_status));
            if (incoming) // else remote will be set when accepted
                iceA->setRemoteFeatures(evalIceFeatures(iceA_status));
            setup_ice(iceA, localAddrs);

            iceA_status.started = false;
        }

        if (types & JingleRtp::Video) {
            iceV = new XMPP::Ice176(this);
            iceV->setLocalFeatures(evalIceFeatures(iceV_status));
            if (incoming) // else remote will be set when accepted
                iceV->setRemoteFeatures(evalIceFeatures(iceV_status));
            setup_ice(iceV, localAddrs);

            iceV_status.started = false;
        }

        XMPP::Ice176::Mode m = incoming ? XMPP::Ice176::Responder : XMPP::Ice176::Initiator;
        if (iceA) {
            printf("starting ice for audio\n");
            iceA->start(m);
        }
        if (iceV) {
            printf("starting ice for video\n");
            iceV->start(m);
        }
    }

    void setup_ice(XMPP::Ice176 *ice, const QList<XMPP::Ice176::LocalAddress> &localAddrs)
    {
        connect(ice, SIGNAL(started()), SLOT(on_ice_started()));
        connect(ice, SIGNAL(error(XMPP::Ice176::Error)), SLOT(ice_error(XMPP::Ice176::Error)));
        connect(ice, SIGNAL(localCandidatesReady(const QList<XMPP::Ice176::Candidate> &)),
                SLOT(ice_localCandidatesReady(const QList<XMPP::Ice176::Candidate> &)));
        connect(ice, &XMPP::Ice176::readyToSendMedia, this, &JingleRtpPrivate::ice_readyToSendMedia,
                Qt::QueuedConnection);
        connect(ice, &XMPP::Ice176::localGatheringComplete, this, &JingleRtpPrivate::ice_localGatheringComplete);

        ice->setProxy(manager->stunProxy);
        if (portReserver)
            ice->setPortReserver(portReserver);

        // QList<XMPP::Ice176::LocalAddress> localAddrs;
        // XMPP::Ice176::LocalAddress addr;

        // FIXME: the following is not true, a local address is not
        //   required, for example if you use TURN with TCP only

        // a local address is required to use ice.  however, if
        //   we don't have a local address, we won't handle it as
        //   an error here.  instead, we'll start Ice176 anyway,
        //   which should immediately error back at us.
        /*if(manager->selfAddr.isNull())
        {
            printf("no self address to use.  this will fail.\n");
            return;
        }

        addr.addr = manager->selfAddr;
        localAddrs += addr;*/
        ice->setLocalAddresses(localAddrs);

        // if an external address is manually provided, then apply
        //   it only to the selfAddr.  FIXME: maybe we should apply
        //   it to all local addresses?
        if (!extAddr.isNull()) {
            QList<XMPP::Ice176::ExternalAddress> extAddrs;
            /*XMPP::Ice176::ExternalAddress eaddr;
            eaddr.base = addr;
            eaddr.addr = extAddr;
            extAddrs += eaddr;*/
            for (const XMPP::Ice176::LocalAddress &la : localAddrs) {
                XMPP::Ice176::ExternalAddress ea;
                ea.base = la;
                ea.addr = extAddr;
                // TODO: assumed direct port mapping. extPort = localPort. It's not that good
                extAddrs += ea;
            }
            ice->setExternalAddresses(extAddrs);
        }

        if (!stunBindAddr.isNull() && stunBindPort > 0)
            ice->setStunBindService(stunBindAddr, stunBindPort);
        if (!stunRelayUdpAddr.isNull() && !manager->stunRelayUdpUser.isEmpty())
            ice->setStunRelayUdpService(stunRelayUdpAddr, stunRelayUdpPort, manager->stunRelayUdpUser,
                                        manager->stunRelayUdpPass.toUtf8());
        if (!stunRelayTcpAddr.isNull() && !manager->stunRelayTcpUser.isEmpty())
            ice->setStunRelayTcpService(stunRelayTcpAddr, stunRelayTcpPort, manager->stunRelayTcpUser,
                                        manager->stunRelayTcpPass.toUtf8());
        ice->setAllowIpExposure(manager->allowIpExposure);

        // RTP+RTCP
        ice->setComponentCount(2);
    }

    void flushLocalCandidates()
    {
        printf("flushing local candidates\n");

        QList<JingleRtpContent> contentList;

        // according to xep-166, creator is always whoever added
        //   the content type, which in our case is always the
        //   initiator

        if ((types & JingleRtp::Audio)
            && (iceA_status.notifyLocalGatheringComplete || !iceA_status.localCandidates.isEmpty())) {

            JingleRtpContent content;
            // if(!incoming)
            content.creator = "initiator";
            // else
            //    content.creator = "responder";
            content.name                    = audioName;
            content.trans.transportType     = iceA_status.transportType;
            content.trans.user              = iceA->localUfrag();
            content.trans.pass              = iceA->localPassword();
            content.trans.candidates        = iceA_status.localCandidates;
            content.trans.gatheringComplete = iceA_status.notifyLocalGatheringComplete;

            iceA_status.localCandidates.clear();
            iceA_status.notifyLocalGatheringComplete = false;

            contentList += content;
        }

        if ((types & JingleRtp::Video)
            && (iceV_status.notifyLocalGatheringComplete || !iceV_status.localCandidates.isEmpty())) {

            JingleRtpContent content;
            // if(!incoming)
            content.creator = "initiator";
            // else
            //    content.creator = "responder";
            content.name                    = videoName;
            content.trans.transportType     = iceV_status.transportType;
            content.trans.user              = iceV->localUfrag();
            content.trans.pass              = iceV->localPassword();
            content.trans.candidates        = iceV_status.localCandidates;
            content.trans.gatheringComplete = iceV_status.notifyLocalGatheringComplete;

            iceV_status.localCandidates.clear();
            iceV_status.notifyLocalGatheringComplete = false;

            contentList += content;
        }

        if (!contentList.isEmpty()) {
            JingleRtpEnvelope envelope;
            envelope.action      = "transport-info";
            envelope.sid         = sid;
            envelope.contentList = contentList;

            JT_JingleRtp *task = new JT_JingleRtp(manager->client->rootTask());
            task->request(peer, envelope);
            task->go(true);
        }
    }

    void flushRemoteCandidates()
    {
        // FIXME: currently, new candidates are ignored after the
        //   session is activated (iceA/iceV are passed to
        //   JingleRtpChannel and our local pointers are nulled).
        //   unfortunately this means we can't upgrade to better
        //   candidates on the fly.

        if (types & JingleRtp::Audio && iceA) {
            iceA->setPeerUfrag(iceA_status.remoteUfrag);
            iceA->setPeerPassword(iceA_status.remotePassword);
            if (!iceA_status.remoteCandidates.isEmpty()) {
                iceA->addRemoteCandidates(iceA_status.remoteCandidates);
                iceA_status.remoteCandidates.clear();
            }
            if (iceA_status.notifyRemoteGatheringComplete) {
                iceA->setRemoteGatheringComplete();
                iceA_status.notifyRemoteGatheringComplete = false;
            }
        }

        if (types & JingleRtp::Video && iceV) {
            iceV->setPeerUfrag(iceV_status.remoteUfrag);
            iceV->setPeerPassword(iceV_status.remotePassword);
            if (!iceV_status.remoteCandidates.isEmpty()) {
                iceV->addRemoteCandidates(iceV_status.remoteCandidates);
                iceV_status.remoteCandidates.clear();
            }
            if (iceV_status.notifyRemoteGatheringComplete) {
                iceV->setRemoteGatheringComplete();
                iceV_status.notifyRemoteGatheringComplete = false;
            }
        }
    }

    void sendInitiate()
    {
        sid = manager->createSid(peer);
        manager->client->jingleManager()->registerExternalSession(sid);

        JingleRtpEnvelope envelope;
        envelope.action    = "session-initiate";
        envelope.initiator = manager->client->jid().full();
        envelope.sid       = sid;

        if (types & JingleRtp::Audio) {
            audioName = "A";

            JingleRtpContent content;
            content.creator = "initiator";
            content.name    = audioName;
            content.senders = "both";

            content.desc.media          = "audio";
            content.desc.payloadTypes   = localAudioPayloadTypes;
            content.trans.transportType = iceA_status.transportType;
            content.trans.user          = iceA->localUfrag();
            content.trans.pass          = iceA->localPassword();
            content.trans.candidates    = iceA_status.localCandidates;
            iceA_status.localCandidates.clear();

            envelope.contentList += content;
        }

        if (types & JingleRtp::Video) {
            videoName = "V";

            JingleRtpContent content;
            content.creator = "initiator";
            content.name    = videoName;
            content.senders = "both";

            content.desc.media          = "video";
            content.desc.payloadTypes   = localVideoPayloadTypes;
            content.trans.transportType = iceV_status.transportType;
            content.trans.user          = iceV->localUfrag();
            content.trans.pass          = iceV->localPassword();
            content.trans.candidates    = iceV_status.localCandidates;
            iceV_status.localCandidates.clear();

            envelope.contentList += content;
        }

        jt = new JT_JingleRtp(manager->client->rootTask());
        connect(jt, &JT_JingleRtp::finished, this, [this]() {
            if (jt->success()) {
                prov_accepted = true;
                flushLocalCandidates();
            } else {
                cleanup();
                emit q->rejected();
            }
        });
        jt->request(peer, envelope);
        jt->go(true);
    }

    void tryAccept()
    {
        if (!local_media_ready || !ice_started || session_accept_sent)
            return;

        JingleRtpEnvelope envelope;
        envelope.action    = "session-accept";
        envelope.responder = manager->client->jid().full();
        envelope.sid       = sid;

        if (types & JingleRtp::Audio) {
            JingleRtpContent content;
            content.creator = "initiator";
            content.name    = audioName;
            content.senders = "both";

            content.desc.media          = "audio";
            content.desc.payloadTypes   = localAudioPayloadTypes;
            content.trans.transportType = iceA_status.transportType;
            content.trans.user          = iceA->localUfrag();
            content.trans.pass          = iceA->localPassword();
            content.trans.candidates    = iceA_status.localCandidates;
            iceA_status.localCandidates.clear();

            envelope.contentList += content;
        }

        if (types & JingleRtp::Video) {
            JingleRtpContent content;
            content.creator = "initiator";
            content.name    = videoName;
            content.senders = "both";

            content.desc.media          = "video";
            content.desc.payloadTypes   = localVideoPayloadTypes;
            content.trans.transportType = iceV_status.transportType;
            content.trans.user          = iceV->localUfrag();
            content.trans.pass          = iceV->localPassword();
            content.trans.candidates    = iceV_status.localCandidates;
            iceV_status.localCandidates.clear();

            envelope.contentList += content;
        }

        session_accept_sent = true;

        JT_JingleRtp *task = new JT_JingleRtp(manager->client->rootTask());
        task->request(peer, envelope);
        connect(task, &JT_JingleRtp::finished, this, [this, task]() {
            if (task->success()) {
                if (iceA)
                    iceA->startChecks();
                if (iceV)
                    iceV->startChecks();
                flushRemoteCandidates();
            } else {
                reject();
                errorCode = JingleRtp::ErrorTimeout;
                emit q->error();
            }
        });
        task->go(true);
    }

    void tryActivated()
    {
        if (session_accept_sent && ice_connected) {
            if (session_activated) {
                printf("warning: attempting to activate an already active session\n");
                return;
            }

            printf("activating!\n");
            session_activated = true;

            if (portReserver)
                portReserver->setParent(nullptr);

            if (iceA) {
                iceA->disconnect(this);
                iceA->setParent(nullptr);
            }
            if (iceV) {
                iceV->disconnect(this);
                iceV->setParent(nullptr);
            }

            rtpChannel->d->setIceObjects(portReserver, iceA, iceV);

            portReserver = nullptr;
            iceA         = nullptr;
            iceV         = nullptr;

            emit q->activated();
        }
    }

private slots:
    void resolver_finished()
    {
        extAddr          = resolver.extAddr;
        stunBindAddr     = resolver.stunBindAddr;
        stunRelayUdpAddr = resolver.stunRelayUdpAddr;
        stunRelayTcpAddr = resolver.stunRelayTcpAddr;

        printf("resolver finished\n");

        start_ice();
    }

    // this happens when when we are ready to send offer/answer to remote. It's already accepted by the user
    void on_ice_started()
    {
        XMPP::Ice176 *ice = static_cast<XMPP::Ice176 *>(sender());

        printf("ice_started\n");

        if (ice == iceA) {
            iceA_status.started = true;

            // audio rtp/rtcp expected to have small packets
            iceA->flagComponentAsLowOverhead(0);
            iceA->flagComponentAsLowOverhead(1);
        } else // iceV
        {
            iceV_status.started = true;

            // video rtcp expected to have small packets
            iceV->flagComponentAsLowOverhead(1);
        }

        ice_started = true;
        if ((types & JingleRtp::Audio) && !iceA_status.started)
            ice_started = false;
        if ((types & JingleRtp::Video) && !iceV_status.started)
            ice_started = false;

        if (!ice_started)
            return;

        // for outbound, send the session-initiate
        if (incoming) {
            tryAccept(); // will flush local candidates
        } else {
            sendInitiate();
        }
    }

    void ice_error(XMPP::Ice176::Error e)
    {
        Q_UNUSED(e);

        errorCode = JingleRtp::ErrorICE;
        emit q->error();
    }

    void ice_localCandidatesReady(const QList<XMPP::Ice176::Candidate> &list)
    {
        XMPP::Ice176 *ice = static_cast<XMPP::Ice176 *>(sender());

        if (ice == iceA) {
            iceA_status.localCandidates += list;
            printf("got more local candidates for audio stream\n");
        } else { // iceV
            iceV_status.localCandidates += list;
            printf("got more local candidates for video stream\n");
        }
        if (session_accept_sent)
            flushLocalCandidates();
    }

    void ice_localGatheringComplete()
    {
        XMPP::Ice176 *ice = static_cast<XMPP::Ice176 *>(sender());
        if (ice == iceA) {
            iceA_status.notifyLocalGatheringComplete = true;
        } else { // iceV
            iceV_status.notifyLocalGatheringComplete = true;
        }
        if (session_accept_sent)
            flushLocalCandidates();
    }

    void ice_readyToSendMedia()
    {
        if (ice_connected) {
            // the signal connection is asynchronous, so by the time of the first etrance to the
            // function we can already have both audio and video ready to send media. So just return
            // in this case.
            return;
        }
        bool allReady = true;

        if (types & JingleRtp::Audio && !iceA->canSendMedia()) {
            allReady = false;
        }

        if (types & JingleRtp::Video && !iceV->canSendMedia()) {
            allReady = false;
        }

        if (allReady) {
            ice_connected = true;
            tryActivated();
        }
    }

    void after_session_accept() { tryActivated(); }
};

JingleRtp::JingleRtp() { d = new JingleRtpPrivate(this); }

JingleRtp::~JingleRtp() { delete d; }

XMPP::Jid JingleRtp::jid() const { return d->peer; }

QList<JingleRtpPayloadType> JingleRtp::remoteAudioPayloadTypes() const { return d->remoteAudioPayloadTypes; }

QList<JingleRtpPayloadType> JingleRtp::remoteVideoPayloadTypes() const { return d->remoteVideoPayloadTypes; }

int JingleRtp::remoteMaximumBitrate() const { return d->remoteMaximumBitrate; }

void JingleRtp::setLocalAudioPayloadTypes(const QList<JingleRtpPayloadType> &types)
{
    d->localAudioPayloadTypes = types;
}

void JingleRtp::setLocalVideoPayloadTypes(const QList<JingleRtpPayloadType> &types)
{
    d->localVideoPayloadTypes = types;
}

void JingleRtp::setLocalMaximumBitrate(int kbps) { d->localMaximumBitrate = kbps; }

void JingleRtp::connectToJid(const XMPP::Jid &jid, JingleRtp::PeerFeatures features)
{
    d->peer         = jid;
    d->peerFeatures = features;
    d->startOutgoing();
}

void JingleRtp::accept(int types) { d->accept(types); }

void JingleRtp::reject() { d->reject(); }

void JingleRtp::localMediaUpdate() { d->localMediaUpdate(); }

JingleRtp::Error JingleRtp::errorCode() const { return d->errorCode; }

JingleRtpChannel *JingleRtp::rtpChannel() { return d->rtpChannel; }

//----------------------------------------------------------------------------
// JingleRtpChannel
//----------------------------------------------------------------------------
JingleRtpChannelPrivate::JingleRtpChannelPrivate(JingleRtpChannel *_q) :
    QObject(_q), q(_q), portReserver(nullptr), iceA(nullptr), iceV(nullptr)
{
    rtpActivityTimer = new QTimer(this);
    connect(rtpActivityTimer, SIGNAL(timeout()), SLOT(rtpActivity_timeout()));
}

JingleRtpChannelPrivate::~JingleRtpChannelPrivate()
{
    if (portReserver) {
        QList<XMPP::Ice176 *> list;

        portReserver->setParent(nullptr);

        if (iceA) {
            iceA->disconnect(this);
            iceA->setParent(nullptr);
            // iceA->deleteLater();
            list += iceA;
        }

        if (iceV) {
            iceV->disconnect(this);
            iceV->setParent(nullptr);
            // iceV->deleteLater();
            list += iceV;
        }

        // pass ownership of portReserver, iceA, and iceV
        IceStopper *iceStopper = new IceStopper;
        iceStopper->start(portReserver, std::move(list));
    }

    rtpActivityTimer->setParent(nullptr);
    rtpActivityTimer->disconnect(this);
    rtpActivityTimer->deleteLater();
}

void JingleRtpChannelPrivate::setIceObjects(XMPP::UdpPortReserver *_portReserver, XMPP::Ice176 *_iceA,
                                            XMPP::Ice176 *_iceV)
{
    if (QThread::currentThread() != thread()) {
        // if called from another thread, safely change ownership
        QMutexLocker locker(&m);

        portReserver = _portReserver;
        iceA         = _iceA;
        iceV         = _iceV;

        if (portReserver)
            portReserver->moveToThread(thread());

        if (iceA) {
            iceA->changeThread(thread());
            connect(iceA, SIGNAL(readyRead(int)), SLOT(ice_readyRead(int)));
            connect(iceA, SIGNAL(datagramsWritten(int, int)), SLOT(ice_datagramsWritten(int, int)));
        }

        if (iceV) {
            iceV->changeThread(thread());
            connect(iceV, SIGNAL(readyRead(int)), SLOT(ice_readyRead(int)));
            connect(iceV, SIGNAL(datagramsWritten(int, int)), SLOT(ice_datagramsWritten(int, int)));
        }

        QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
    } else {
        portReserver = _portReserver;
        iceA         = _iceA;
        iceV         = _iceV;

        if (iceA) {
            connect(iceA, SIGNAL(readyRead(int)), SLOT(ice_readyRead(int)));
            connect(iceA, SIGNAL(datagramsWritten(int, int)), SLOT(ice_datagramsWritten(int, int)));
        }

        if (iceV) {
            connect(iceV, SIGNAL(readyRead(int)), SLOT(ice_readyRead(int)));
            connect(iceV, SIGNAL(datagramsWritten(int, int)), SLOT(ice_datagramsWritten(int, int)));
        }

        start();
    }
}

void JingleRtpChannelPrivate::restartRtpActivityTimer()
{
    // if we go 5 seconds without an RTP packet, then that's
    //   pretty bad
    rtpActivityTimer->start(5000);
}

void JingleRtpChannelPrivate::start()
{
    if (portReserver)
        portReserver->setParent(this);
    if (iceA)
        iceA->setParent(this);
    if (iceV)
        iceV->setParent(this);
    restartRtpActivityTimer();
}

void JingleRtpChannelPrivate::ice_readyRead(int componentIndex)
{
    XMPP::Ice176 *ice = static_cast<XMPP::Ice176 *>(sender());

    if (ice == iceA && componentIndex == 0)
        restartRtpActivityTimer();

    if (ice == iceA) {
        while (iceA->hasPendingDatagrams(componentIndex)) {
            JingleRtp::RtpPacket packet;
            packet.type       = JingleRtp::Audio;
            packet.portOffset = componentIndex;
            packet.value      = iceA->readDatagram(componentIndex);
            in += packet;
        }
    } else // iceV
    {
        while (iceV->hasPendingDatagrams(componentIndex)) {
            JingleRtp::RtpPacket packet;
            packet.type       = JingleRtp::Video;
            packet.portOffset = componentIndex;
            packet.value      = iceV->readDatagram(componentIndex);
            in += packet;
        }
    }

    emit q->readyRead();
}

void JingleRtpChannelPrivate::ice_datagramsWritten(int componentIndex, int count)
{
    Q_UNUSED(componentIndex);

    emit q->packetsWritten(count);
}

void JingleRtpChannelPrivate::rtpActivity_timeout()
{
    printf("warning: 5 seconds passed without receiving audio RTP\n");
}

JingleRtpChannel::JingleRtpChannel() { d = new JingleRtpChannelPrivate(this); }

JingleRtpChannel::~JingleRtpChannel() { delete d; }

bool JingleRtpChannel::packetsAvailable() const { return !d->in.isEmpty(); }

JingleRtp::RtpPacket JingleRtpChannel::read() { return d->in.takeFirst(); }

void JingleRtpChannel::write(const JingleRtp::RtpPacket &packet)
{
    QMutexLocker locker(&d->m);

    if (packet.type == JingleRtp::Audio && d->iceA)
        d->iceA->writeDatagram(packet.portOffset, packet.value);
    else if (packet.type == JingleRtp::Video && d->iceV)
        d->iceV->writeDatagram(packet.portOffset, packet.value);
}

//----------------------------------------------------------------------------
// JingleRtpManager
//----------------------------------------------------------------------------
JingleRtpManagerPrivate::JingleRtpManagerPrivate(XMPP::Client *_client, JingleRtpManager *_q) :
    QObject(_q), q(_q), client(_client), stunBindPort(-1), stunRelayUdpPort(-1), stunRelayTcpPort(-1), basePort(-1)
{
    push_task = new JT_PushJingleRtp(client->rootTask());
    connect(push_task, SIGNAL(incomingRequest(const XMPP::Jid &, const QString &, const JingleRtpEnvelope &)),
            SLOT(push_task_incomingRequest(const XMPP::Jid &, const QString &, const JingleRtpEnvelope &)));
}

JingleRtpManagerPrivate::~JingleRtpManagerPrivate() { delete push_task; }

QString JingleRtpManagerPrivate::createSid(const XMPP::Jid &peer) const
{
    while (1) {
        QString out = XMPP::IceAgent::randomCredential(16);

        bool found = false;
        for (int n = 0; n < sessions.count(); ++n) {
            if (sessions[n]->d->peer == peer && sessions[n]->d->sid == out) {
                found = true;
                break;
            }
        }

        if (!found)
            return out;
    }
}

void JingleRtpManagerPrivate::unlink(JingleRtp *sess) { sessions.removeAll(sess); }

void JingleRtpManagerPrivate::push_task_incomingRequest(const XMPP::Jid &from, const QString &iq_id,
                                                        const JingleRtpEnvelope &envelope)
{
    printf("incoming request: [%s]\n", qPrintable(envelope.action));

    // don't allow empty sid
    if (envelope.sid.isEmpty()) {
        push_task->respondError(from, iq_id, 400, QString());
        return;
    }

    if (envelope.action == "session-initiate") {
        int at = -1;
        for (int n = 0; n < sessions.count(); ++n) {
            if (sessions[n]->d->peer == from && sessions[n]->d->sid == envelope.sid) {
                at = n;
                break;
            }
        }

        // duplicate session
        if (at != -1) {
            push_task->respondError(from, iq_id, 400, QString());
            return;
        }

        JingleRtp *sess   = new JingleRtp;
        sess->d->manager  = this;
        sess->d->incoming = true;
        sess->d->peer     = from;
        sess->d->sid      = envelope.sid;
        sessions += sess;
        printf("new initiate, from=[%s] sid=[%s]\n", qPrintable(from.full()), qPrintable(envelope.sid));
        if (!sess->d->incomingRequest(iq_id, envelope)) {
            delete sess;
            push_task->respondError(from, iq_id, 400, QString());
            return;
        }
        push_task->respondSuccess(from, iq_id); // by xep we should also check if `from` jid is allowed
                                                // or if we lack of resources.

        pending += sess;
        emit q->incomingReady();
    } else {
        int at = -1;
        for (int n = 0; n < sessions.count(); ++n) {
            if (sessions[n]->d->peer == from && sessions[n]->d->sid == envelope.sid) {
                at = n;
                break;
            }
        }

        // session not found
        if (at == -1) {
            push_task->respondError(from, iq_id, 400, QString());
            return;
        }

        sessions[at]->d->incomingRequest(iq_id, envelope);
    }
}

JingleRtpManager::JingleRtpManager(XMPP::Client *client) : QObject(nullptr)
{
    d = new JingleRtpManagerPrivate(client, this);
}

JingleRtpManager::~JingleRtpManager() { delete d; }

JingleRtp *JingleRtpManager::createOutgoing()
{
    JingleRtp *sess   = new JingleRtp;
    sess->d->manager  = d;
    sess->d->incoming = false;
    d->sessions += sess;
    return sess;
}

JingleRtp *JingleRtpManager::takeIncoming() { return d->pending.takeFirst(); }

void JingleRtpManager::setSelfAddress(const QHostAddress &addr) { d->selfAddr = addr; }

void JingleRtpManager::setExternalAddress(const QString &host) { d->extHost = host; }

void JingleRtpManager::setStunBindService(const QString &host, int port)
{
    d->stunBindHost = host;
    d->stunBindPort = port;
}

void JingleRtpManager::setStunRelayUdpService(const QString &host, int port, const QString &user, const QString &pass)
{
    d->stunRelayUdpHost = host;
    d->stunRelayUdpPort = port;
    d->stunRelayUdpUser = user;
    d->stunRelayUdpPass = pass;
}

void JingleRtpManager::setStunRelayTcpService(const QString &host, int port,
                                              const XMPP::AdvancedConnector::Proxy &proxy, const QString &user,
                                              const QString &pass)
{
    d->stunRelayTcpHost = host;
    d->stunRelayTcpPort = port;
    d->stunRelayTcpUser = user;
    d->stunRelayTcpPass = pass;

    XMPP::TurnClient::Proxy tproxy;

    if (proxy.type() == XMPP::AdvancedConnector::Proxy::HttpConnect) {
        tproxy.setHttpConnect(proxy.host(), proxy.port());
        tproxy.setUserPass(proxy.user(), proxy.pass());
    } else if (proxy.type() == XMPP::AdvancedConnector::Proxy::Socks) {
        tproxy.setSocks(proxy.host(), proxy.port());
        tproxy.setUserPass(proxy.user(), proxy.pass());
    }

    d->stunProxy = tproxy;
}

void JingleRtpManager::setAllowIpExposure(bool allow) { d->allowIpExposure = allow; }

void JingleRtpManager::setBasePort(int port) { d->basePort = port; }

#include "jinglertp.moc"
