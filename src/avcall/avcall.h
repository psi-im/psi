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

#ifndef AVCALL_H
#define AVCALL_H

#include <QObject>
#include "xmpp.h"

class QHostAddress;

namespace XMPP
{
	class Jid;
}

namespace PsiMedia
{
	class VideoWidget;
}

class PsiAccount;

class AvCallPrivate;
class AvCallManagerPrivate;

class AvCall : public QObject
{
	Q_OBJECT

public:
	enum Mode
	{
		Audio,
		Video,
		Both
	};

	AvCall(const AvCall &from);
	~AvCall();

	XMPP::Jid jid() const;
	Mode mode() const;

	void connectToJid(const XMPP::Jid &jid, Mode mode, int kbps = -1);
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

	AvCallPrivate *d;
};

class AvCallManager : public QObject
{
	Q_OBJECT

public:
	AvCallManager(PsiAccount *pa);
	~AvCallManager();

	AvCall *createOutgoing();
	AvCall *takeIncoming();

	static void config();
	static bool isSupported();
	static bool isVideoSupported();

	void setSelfAddress(const QHostAddress &addr);
	void setStunBindService(const QString &host, int port);
	void setStunRelayUdpService(const QString &host, int port, const QString &user, const QString &pass);
	void setStunRelayTcpService(const QString &host, int port, const XMPP::AdvancedConnector::Proxy &proxy, const QString &user, const QString &pass);

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

#endif
