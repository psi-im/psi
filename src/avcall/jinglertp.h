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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef JINGLERTP_H
#define JINGLERTP_H

#include "xmpp.h"
#include "jinglertptasks.h"

class JingleRtpChannel;
class JingleRtpPrivate;
class JingleRtpChannelPrivate;
class JingleRtpManagerPrivate;

class JingleRtp : public QObject
{
	Q_OBJECT

public:
	enum Error
	{
		ErrorGeneric,
		ErrorTimeout,
		ErrorICE
	};

	enum Type
	{
		Audio = 0x01,
		Video = 0x02
	};

	class RtpPacket
	{
	public:
		Type type;
		int portOffset;
		QByteArray value;
	};

	~JingleRtp();

	XMPP::Jid jid() const;
	QList<JingleRtpPayloadType> remoteAudioPayloadTypes() const;
	QList<JingleRtpPayloadType> remoteVideoPayloadTypes() const;
	int remoteMaximumBitrate() const;

	void setLocalAudioPayloadTypes(const QList<JingleRtpPayloadType> &types);
	void setLocalVideoPayloadTypes(const QList<JingleRtpPayloadType> &types);
	void setLocalMaximumBitrate(int kbps);

	void connectToJid(const XMPP::Jid &jid);
	void accept(int types); // intended types, so ICE knows what to do
	void reject();

	// indicates that local media settings have changed.  note that for
	//   incoming sessions, this MUST be called.  local media settings
	//   are not assumed to be ready when accept() is called (basically
	//   this allows ICE negotiation to run in parallel to the RTP engine
	//   initialization).
	void localMediaUpdate();

	Error errorCode() const;

	// this object is valid at construction time and initially lives in
	//   JingleRtp's thread.  it can be moved to another thread as long
	//   it is moved back to JingleRtp's thread before destructing
	//   JingleRtp.
	JingleRtpChannel *rtpChannel();

signals:
	void rejected();
	void error();
	void activated();

	// indicates that remote media settings have changed.  note that for
	//   outgoing sessions, this must be listened to in order to get the
	//   initial values.
	void remoteMediaUpdated();

private:
	Q_DISABLE_COPY(JingleRtp);

	friend class JingleRtpPrivate;
	friend class JingleRtpManager;
	friend class JingleRtpManagerPrivate;
	JingleRtp();

	JingleRtpPrivate *d;
};

class JingleRtpChannel : public QObject
{
	Q_OBJECT

public:
	bool packetsAvailable() const;
	JingleRtp::RtpPacket read();
	void write(const JingleRtp::RtpPacket &packet);

signals:
	void readyRead();

	// note: this says nothing about the order packets were written
	void packetsWritten(int count);

private:
	Q_DISABLE_COPY(JingleRtpChannel);

	friend class JingleRtpChannelPrivate;
	friend class JingleRtpPrivate;
	JingleRtpChannel();
	~JingleRtpChannel();

	JingleRtpChannelPrivate *d;
};

class JingleRtpManager : public QObject
{
	Q_OBJECT

public:
	JingleRtpManager(XMPP::Client *client);
	~JingleRtpManager();

	JingleRtp *createOutgoing();
	JingleRtp *takeIncoming();

	void setSelfAddress(const QHostAddress &addr);
	void setExternalAddress(const QString &host); // resolved locally
	void setStunBindService(const QString &host, int port);
	void setStunRelayUdpService(const QString &host, int port, const QString &user, const QString &pass);
	void setStunRelayTcpService(const QString &host, int port, const XMPP::AdvancedConnector::Proxy &proxy, const QString &user, const QString &pass);
	void setBasePort(int port);

signals:
	void incomingReady();

private:
	Q_DISABLE_COPY(JingleRtpManager);

	friend class JingleRtpManagerPrivate;
	friend class JingleRtp;
	friend class JingleRtpPrivate;

	JingleRtpManagerPrivate *d;
};

#endif
