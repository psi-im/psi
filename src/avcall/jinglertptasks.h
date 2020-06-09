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

#ifndef JINGLERTPTASKS_H
#define JINGLERTPTASKS_H

#include "iris/ice176.h"
#include "xmpp_jid.h"
#include "xmpp_task.h"

#include <QDomElement>
#include <QHostAddress>

class JingleRtpPayloadType {
public:
    class Parameter {
    public:
        QString name;
        QString value;
    };

    int              id;
    QString          name;
    int              clockrate;
    int              channels;
    int              ptime;
    int              maxptime;
    QList<Parameter> parameters;

    JingleRtpPayloadType() : id(-1), clockrate(-1), channels(-1), ptime(-1), maxptime(-1) { }
};

class JingleRtpCrypto {
public:
    QString suite;
    QString keyParams;
    QString sessionParams;
    int     tag;
};

class JingleRtpDesc {
public:
    QString                     media;
    bool                        haveSsrc;
    quint32                     ssrc;
    QList<JingleRtpPayloadType> payloadTypes;
    int                         bitrate; // in Kbps
    bool                        encryptionRequired;
    QList<JingleRtpCrypto>      cryptoList;

    JingleRtpDesc() : haveSsrc(false), ssrc(0), bitrate(-1), encryptionRequired(false) { }
};

class JingleRtpRemoteCandidate {
public:
    int          component;
    QHostAddress addr;
    int          port;

    JingleRtpRemoteCandidate() : component(-1), port(-1) { }
};

class JingleRtpTrans {
public:
    enum Transport { IceUdp, Ice };

    QString                         user;
    QString                         pass;
    bool                            ice2              = false;
    bool                            gatheringComplete = false;
    Transport                       transportType     = IceUdp;
    QList<XMPP::Ice176::Candidate>  candidates;
    QList<JingleRtpRemoteCandidate> remoteCandidates;
};

class JingleRtpContent {
public:
    QString creator;
    QString disposition;
    QString name;
    QString senders;

    JingleRtpDesc  desc;
    JingleRtpTrans trans;
};

class JingleRtpReason {
public:
    enum Condition {
        AlternativeSession,
        Busy,
        Cancel,
        ConnectivityError,
        Decline,
        Expired,
        FailedApplication,
        FailedTransport,
        GeneralError,
        Gone,
        IncompatibleParameters,
        MediaError,
        SecurityError,
        Success,
        Timeout,
        UnsupportedApplications,
        UnsupportedTransports
    };

    Condition condition;
    QString   text;

    JingleRtpReason() : condition((Condition)-1) { }
};

class JingleRtpEnvelope {
public:
    QString action;
    QString initiator;
    QString responder;
    QString sid;

    QList<JingleRtpContent> contentList;

    JingleRtpReason reason;
};

class JT_JingleRtp : public XMPP::Task {
    Q_OBJECT

public:
    explicit JT_JingleRtp(XMPP::Task *parent);
    ~JT_JingleRtp() override;

    void request(const XMPP::Jid &to, const JingleRtpEnvelope &envelope);

    void onGo() override;
    bool take(const QDomElement &x) override;

private:
    QDomElement iq_;
    XMPP::Jid   to_;
};

class JT_PushJingleRtp : public XMPP::Task {
    Q_OBJECT

public:
    explicit JT_PushJingleRtp(XMPP::Task *parent);
    ~JT_PushJingleRtp() override;

    void respondSuccess(const XMPP::Jid &to, const QString &id);
    void respondError(const XMPP::Jid &to, const QString &id, int code, const QString &str);

    bool take(const QDomElement &e) override;

signals:
    void incomingRequest(const XMPP::Jid &from, const QString &iq_id, const JingleRtpEnvelope &envelope);
};

#endif // JINGLERTPTASKS_H
