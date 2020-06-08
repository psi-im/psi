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

#ifndef AVCALL_H
#define AVCALL_H

#include "xmpp.h"

#include <QObject>

class AvCallManagerPrivate;
class AvCallPrivate;
class PsiAccount;
class QHostAddress;

namespace PsiMedia {
class VideoWidget;
}

namespace XMPP {
class Jid;
}

class AvCall : public QObject {
    Q_OBJECT

public:
    enum Mode { Audio, Video, Both };

    enum PeerFeature { IceTransport = 0x1, IceUdpTransport = 0x2 };
    Q_DECLARE_FLAGS(PeerFeatures, PeerFeature)

    AvCall(const AvCall &from);
    ~AvCall() override;

    XMPP::Jid jid() const;
    Mode      mode() const;

    void connectToJid(const XMPP::Jid &jid, Mode mode, int kbps = -1, PeerFeatures features = IceUdpTransport);
    void accept(Mode mode, int kbps = -1);
    void reject();

    void setIncomingVideo(PsiMedia::VideoWidget *widget);

    QString errorString() const;

    // if we use deleteLater() on a call, then it won't detach from the
    //   manager until the deletion resolves.  use unlink() to immediately
    //   detach, and then call deleteLater().
    void unlink();

signals:
    void activated();
    void error();

private:
    friend class AvCallPrivate;
    friend class AvCallManager;
    friend class AvCallManagerPrivate;
    AvCall();

    AvCallPrivate *d {};
};
Q_DECLARE_OPERATORS_FOR_FLAGS(AvCall::PeerFeatures)

class AvCallManager : public QObject {
    Q_OBJECT

public:
    explicit AvCallManager(PsiAccount *pa);
    ~AvCallManager() override;

    AvCall *createOutgoing();
    AvCall *takeIncoming();

    static void config();
    static bool isSupported();

    void setSelfAddress(const QHostAddress &addr);
    void setStunBindService(const QString &host, int port);
    void setStunRelayUdpService(const QString &host, int port, const QString &user, const QString &pass);
    void setStunRelayTcpService(const QString &host, int port, const XMPP::AdvancedConnector::Proxy &proxy,
                                const QString &user, const QString &pass);
    void setAllowIpExposure(bool allow = true);

    static void setBasePort(int port);
    static void setExternalAddress(const QString &host);
    static void setAudioOutDevice(const QString &id);
    static void setAudioInDevice(const QString &id);
    static void setVideoInDevice(const QString &id);

signals:
    void incomingReady();

private:
    friend class AvCallManagerPrivate;
    friend class AvCall;
    friend class AvCallPrivate;

    AvCallManagerPrivate *d;
};

#endif // AVCALL_H
