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

#include "avcall.h"

#include <stdio.h>
#include <stdlib.h>
#include <QCoreApplication>
#include <QLibrary>
#include <QDir>
#include "xmpp_jid.h"
#include "jinglertp.h"
#include "../psimedia/psimedia.h"
#include "applicationinfo.h"
#include "psiaccount.h"

class Configuration
{
public:
	bool liveInput;
	QString audioOutDeviceId, audioInDeviceId, videoInDeviceId;
	QString file;
	bool loopFile;
	PsiMedia::AudioParams audioParams;
	PsiMedia::VideoParams videoParams;

	int basePort;
	QString extHost;

	Configuration() :
		liveInput(false),
		loopFile(false),
		basePort(-1)
	{
	}
};

// get default settings
static Configuration getDefaultConfiguration()
{
	Configuration config;
	config.liveInput = true;
	config.loopFile = true;
	return config;
}

static Configuration *g_config = 0;

static void ensureConfig()
{
	if(!g_config)
	{
		g_config = new Configuration;
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
	if(!dir.cd(relpath))
		return QString();
	foreach(const QString &fileName, dir.entryList())
	{
		if(fileName.contains(basename))
		{
			QString filePath = dir.filePath(fileName);
			if(QLibrary::isLibrary(filePath))
				return filePath;
		}
	}
	return QString();
}
#endif

static bool g_loaded = false;

static void ensureLoaded()
{
	if(!g_loaded)
	{
#ifndef GSTPROVIDER_STATIC
		QString pluginFile;
		QString resourcePath;

		pluginFile = qgetenv("PSI_MEDIA_PLUGIN");
		if(pluginFile.isEmpty())
		{
#if defined(Q_OS_WIN)
			pluginFile = findPlugin(".", "gstprovider");
			resourcePath = QCoreApplication::applicationDirPath() + "/gstreamer-0.10";
#elif defined(Q_OS_MAC)
			pluginFile = findPlugin("../Plugins", "gstprovider");
			resourcePath = QCoreApplication::applicationDirPath() + "/../Frameworks/gstreamer-0.10";
#else
			pluginFile = findPlugin(ApplicationInfo::libDir() + "/plugins", "gstprovider");
#endif
		}

		PsiMedia::PluginResult r = PsiMedia::loadPlugin(pluginFile, resourcePath);
		if(r == PsiMedia::PluginSuccess)
			g_loaded = true;
#else
		g_loaded = true;
#endif
		if(g_loaded)
			ensureConfig();
	}
}

static JingleRtpPayloadType payloadInfoToPayloadType(const PsiMedia::PayloadInfo &pi)
{
	JingleRtpPayloadType out;
	out.id = pi.id();
	out.name = pi.name();
	out.clockrate = pi.clockrate();
	out.channels = pi.channels();
	out.ptime = pi.ptime();
	out.maxptime = pi.maxptime();
	foreach(const PsiMedia::PayloadInfo::Parameter &pip, pi.parameters())
	{
		JingleRtpPayloadType::Parameter ptp;
		ptp.name = pip.name;
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
	foreach(const JingleRtpPayloadType::Parameter &ptp, pt.parameters)
	{
		PsiMedia::PayloadInfo::Parameter pip;
		pip.name = ptp.name;
		pip.value = ptp.value;
		list += pip;
	}
	out.setParameters(list);
	return out;
}

//----------------------------------------------------------------------------
// AvCall
//----------------------------------------------------------------------------
class AvCallManagerPrivate : public QObject
{
	Q_OBJECT

public:
	AvCallManager *q;
	PsiAccount *pa;
	JingleRtpManager *rtpManager;
	QList<AvCall*> sessions;
	QList<AvCall*> pending;

	AvCallManagerPrivate(PsiAccount *_pa, AvCallManager *_q);
	~AvCallManagerPrivate();

	void unlink(AvCall *call);

private slots:
	void rtp_incomingReady();
};

class AvCallPrivate : public QObject
{
	Q_OBJECT

public:
	AvCall *q;
	AvCallManagerPrivate *manager;
	bool incoming;
	JingleRtp *sess;
	PsiMedia::RtpSession rtp;
	XMPP::Jid peer;
	AvCall::Mode mode;
	int bitrate;
	bool allowVideo;
	QString errorString;
	bool transmitAudio;
	bool transmitVideo;
	bool transmitting;

	AvCallPrivate(AvCall *_q) :
		QObject(_q),
		q(_q),
		manager(0),
		sess(0),
		transmitAudio(false),
		transmitVideo(false),
		transmitting(false)
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
		manager->unlink(q);
	}

	void startOutgoing()
	{
		manager->rtpManager->setBasePort(g_config->basePort);
		manager->rtpManager->setExternalAddress(g_config->extHost);

		start_rtp();
	}

	bool initIncoming()
	{
		setup_sess();

		// JingleRtp guarantees there will be at least one of audio or video
		bool offeredAudio = false;
		bool offeredVideo = false;
		if(!sess->remoteAudioPayloadTypes().isEmpty())
			offeredAudio = true;
		if(allowVideo && !sess->remoteVideoPayloadTypes().isEmpty())
			offeredVideo = true;

		if(offeredAudio && offeredVideo)
			mode = AvCall::Both;
		else if(offeredAudio)
			mode = AvCall::Audio;
		else if(offeredVideo)
			mode = AvCall::Video;
		else
		{
			// this could happen if only video is offered but
			//   we don't allow it
			return false;
		}

		return true;
	}

	void accept()
	{
		manager->rtpManager->setBasePort(g_config->basePort);
		manager->rtpManager->setExternalAddress(g_config->extHost);

		// kick off the acceptance negotiation while simultaneously
		//   initializing the rtp engine.  note that session-accept
		//   won't actually get sent to the peer until we call
		//   localMediaUpdated()
		int types;
		if(mode == AvCall::Both)
			types = JingleRtp::Audio | JingleRtp::Video;
		else if(mode == AvCall::Audio)
			types = JingleRtp::Audio;
		else // Video
			types = JingleRtp::Video;

		sess->accept(types);
		start_rtp();
	}

	void reject()
	{
		if(sess)
			sess->reject();
		cleanup();
	}

private:
	static QString rtpSessionErrorToString(PsiMedia::RtpSession::Error e)
	{
		QString str;
		switch(e)
		{
			case PsiMedia::RtpSession::ErrorSystem:
				str = tr("System error"); break;
			case PsiMedia::RtpSession::ErrorCodec:
				str = tr("Codec error"); break;
			default: // generic
				str = tr("Generic error"); break;
		}
		return str;
	}

	void cleanup()
	{
		rtp.reset();

		delete sess;
		sess = 0;
	}

	void start_rtp()
	{
		Configuration &config = *g_config;

		transmitAudio = false;
		transmitVideo = false;

		if(config.liveInput)
		{
			if(config.audioInDeviceId.isEmpty() && config.videoInDeviceId.isEmpty())
			{
				errorString = tr("Cannot call without selecting a device.  Do you have a microphone?  Check the Psi options.");
				cleanup();
				emit q->error();
				return;
			}

			if((mode == AvCall::Audio || mode == AvCall::Both) && !config.audioInDeviceId.isEmpty())
			{
				rtp.setAudioInputDevice(config.audioInDeviceId);
				transmitAudio = true;
			}
			else
				rtp.setAudioInputDevice(QString());

			if((mode == AvCall::Video || mode == AvCall::Both) && !config.videoInDeviceId.isEmpty() && allowVideo)
			{
				rtp.setVideoInputDevice(config.videoInDeviceId);
				transmitVideo = true;
			}
			else
				rtp.setVideoInputDevice(QString());
		}
		else // non-live (file) input
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

		if(!config.audioOutDeviceId.isEmpty())
			rtp.setAudioOutputDevice(config.audioOutDeviceId);

		// media types are flagged by params, even if empty
		QList<PsiMedia::AudioParams> audioParamsList;
		if(transmitAudio)
			audioParamsList += PsiMedia::AudioParams();
		rtp.setLocalAudioPreferences(audioParamsList);

		QList<PsiMedia::VideoParams> videoParamsList;
		if(transmitVideo)
			videoParamsList += PsiMedia::VideoParams();
		rtp.setLocalVideoPreferences(videoParamsList);

		// for incoming sessions, we have the remote media info at
		//   the start, so use it
		if(incoming)
			setup_remote_media();

		if(bitrate != -1)
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
		connect(sess, SIGNAL(readyReadRtp()), SLOT(sess_readyReadRtp()));
		connect(sess, SIGNAL(rtpWritten(int)), SLOT(sess_rtpWritten(int)));
	}

	void setup_remote_media()
	{
		if(transmitAudio)
		{
			QList<JingleRtpPayloadType> payloadTypes = sess->remoteAudioPayloadTypes();
			QList<PsiMedia::PayloadInfo> list;
			if(!payloadTypes.isEmpty())
				list += payloadTypeToPayloadInfo(payloadTypes.first());
			rtp.setRemoteAudioPreferences(list);
		}

		if(transmitVideo)
		{
			QList<JingleRtpPayloadType> payloadTypes = sess->remoteVideoPayloadTypes();
			QList<PsiMedia::PayloadInfo> list;
			if(!payloadTypes.isEmpty())
				list += payloadTypeToPayloadInfo(payloadTypes.first());
			rtp.setRemoteVideoPreferences(list);
		}

		// FIXME: if the remote side doesn't support a media type,
		//   then we need to downgrade locally
	}

private slots:
	void rtp_started()
	{
		printf("rtp_started\n");

		PsiMedia::PayloadInfo audio, *pAudio;
		PsiMedia::PayloadInfo video, *pVideo;

		pAudio = 0;
		pVideo = 0;
		if(transmitAudio)
		{
			// confirm transmitting of audio is actually possible,
			//   in the case that a file is used as input
			if(rtp.canTransmitAudio())
			{
				audio = rtp.localAudioPayloadInfo().first();
				pAudio = &audio;
			}
			else
				transmitAudio = false;
		}
		if(transmitVideo)
		{
			// same for video
			if(rtp.canTransmitVideo())
			{
				video = rtp.localVideoPayloadInfo().first();
				pVideo = &video;
			}
			else
				transmitVideo = false;
		}

		if(transmitAudio)
			connect(rtp.audioRtpChannel(), SIGNAL(readyRead()), SLOT(rtp_audio_readyRead()));

		if(transmitVideo)
			connect(rtp.videoRtpChannel(), SIGNAL(readyRead()), SLOT(rtp_video_readyRead()));

		if(transmitAudio && transmitVideo)
			mode = AvCall::Both;
		else if(transmitAudio && !transmitVideo)
			mode = AvCall::Audio;
		else if(transmitVideo && !transmitAudio)
			mode = AvCall::Video;
		else
		{
			// can't happen?
			Q_ASSERT(0);
		}

		if(!incoming)
		{
			sess = manager->rtpManager->createOutgoing();
			setup_sess();
		}

		if(pAudio)
		{
			JingleRtpPayloadType pt = payloadInfoToPayloadType(*pAudio);
			sess->setLocalAudioPayloadTypes(QList<JingleRtpPayloadType>() << pt);
		}

		if(pVideo)
		{
			JingleRtpPayloadType pt = payloadInfoToPayloadType(*pVideo);
			sess->setLocalVideoPayloadTypes(QList<JingleRtpPayloadType>() << pt);
		}

		if(!incoming)
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

	void rtp_audio_readyRead()
	{
		while(rtp.audioRtpChannel()->packetsAvailable() > 0)
		{
			PsiMedia::RtpPacket packet = rtp.audioRtpChannel()->read();

			JingleRtp::RtpPacket jpacket;
			jpacket.type = JingleRtp::Audio;
			jpacket.portOffset = packet.portOffset();
			jpacket.value = packet.rawValue();

			if(sess)
				sess->writeRtp(jpacket);
		}
	}

	void rtp_video_readyRead()
	{
		while(rtp.videoRtpChannel()->packetsAvailable() > 0)
		{
			PsiMedia::RtpPacket packet = rtp.videoRtpChannel()->read();

			JingleRtp::RtpPacket jpacket;
			jpacket.type = JingleRtp::Video;
			jpacket.portOffset = packet.portOffset();
			jpacket.value = packet.rawValue();

			if(sess)
				sess->writeRtp(jpacket);
		}
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
		if(e == JingleRtp::ErrorTimeout)
		{
			errorString = tr("Call negotiation timed out.");
			cleanup();
		}
		else if(e == JingleRtp::ErrorICE)
		{
			errorString = tr("Unable to establish peer-to-peer connection.");
			reject();
		}
		else
		{
			errorString = tr("Call negotiation failed.");
			cleanup();
		}

		emit q->error();
	}

	void sess_activated()
	{
		if(transmitAudio)
			rtp.transmitAudio();
		if(transmitVideo)
			rtp.transmitVideo();

		transmitting = true;
		emit q->activated();
	}

	void sess_remoteMediaUpdated()
	{
		setup_remote_media();
		rtp.updatePreferences();
	}

	void sess_readyReadRtp()
	{
		while(sess->rtpAvailable())
		{
			JingleRtp::RtpPacket jpacket = sess->readRtp();

			if(jpacket.type == JingleRtp::Audio)
				rtp.audioRtpChannel()->write(PsiMedia::RtpPacket(jpacket.value, jpacket.portOffset));
			else if(jpacket.type == JingleRtp::Video)
				rtp.videoRtpChannel()->write(PsiMedia::RtpPacket(jpacket.value, jpacket.portOffset));
		}
	}

	void sess_rtpWritten(int count)
	{
		Q_UNUSED(count);

		// nothing
	}
};

AvCall::AvCall()
{
	d = new AvCallPrivate(this);
}

AvCall::AvCall(const AvCall &from) :
	QObject(0)
{
	Q_UNUSED(from);
	fprintf(stderr, "AvCall copy not supported\n");
	abort();
}

AvCall::~AvCall()
{
	delete d;
}

XMPP::Jid AvCall::jid() const
{
	if(d->sess)
		return d->sess->jid();
	else
		return XMPP::Jid();
}

AvCall::Mode AvCall::mode() const
{
	return d->mode;
}

void AvCall::connectToJid(const XMPP::Jid &jid, Mode mode, int kbps)
{
	d->peer = jid;
	d->mode = mode;
	d->bitrate = kbps;
	d->startOutgoing();
}

void AvCall::accept(Mode mode, int kbps)
{
	d->mode = mode;
	d->bitrate = kbps;
	d->accept();
}

void AvCall::reject()
{
	d->reject();
}

void AvCall::setIncomingVideo(PsiMedia::VideoWidget *widget)
{
	d->rtp.setVideoOutputWidget(widget);
}

QString AvCall::errorString() const
{
	return d->errorString;
}

//----------------------------------------------------------------------------
// AvCallManager
//----------------------------------------------------------------------------
AvCallManagerPrivate::AvCallManagerPrivate(PsiAccount *_pa, AvCallManager *_q) :
	QObject(_q),
	q(_q),
	pa(_pa)
{
	rtpManager = new JingleRtpManager(pa->client());
	connect(rtpManager, SIGNAL(incomingReady()), SLOT(rtp_incomingReady()));
}

AvCallManagerPrivate::~AvCallManagerPrivate()
{
	delete rtpManager;
}

void AvCallManagerPrivate::unlink(AvCall *call)
{
	sessions.removeAll(call);
}

void AvCallManagerPrivate::rtp_incomingReady()
{
	AvCall *call = new AvCall;
	call->d->manager = this;
	call->d->incoming = true;
	call->d->sess = rtpManager->takeIncoming();
	sessions += call;
	if(!call->d->initIncoming())
	{
		call->d->sess->reject();
		delete call->d->sess;
		call->d->sess = 0;
		delete call;
		return;
	}

	pending += call;
	emit q->incomingReady();
}

AvCallManager::AvCallManager(PsiAccount *pa) :
	QObject(0)
{
	d = new AvCallManagerPrivate(pa, this);
}

AvCallManager::~AvCallManager()
{
	delete d;
}

AvCall *AvCallManager::createOutgoing()
{
	AvCall *call = new AvCall;
	call->d->manager = d;
	call->d->incoming = false;
	return call;
}

AvCall *AvCallManager::takeIncoming()
{
	return d->pending.takeFirst();
}

void AvCallManager::config()
{
	// TODO: remove this function?
}

bool AvCallManager::isSupported()
{
	ensureLoaded();
	return PsiMedia::isSupported();
}

bool AvCallManager::isVideoSupported()
{
	if(!isSupported())
		return false;

	if(!QString::fromLatin1(qgetenv("PSI_ENABLE_VIDEO")).isEmpty())
		return true;
	else
		return false;
}

void AvCallManager::setSelfAddress(const QHostAddress &addr)
{
	d->rtpManager->setSelfAddress(addr);
}

void AvCallManager::setStunHost(const QString &host, int port)
{
	d->rtpManager->setStunHost(host, port);
}

void AvCallManager::setBasePort(int port)
{
	if(port == 0)
		port = -1;
	g_config->basePort = port;
}

void AvCallManager::setExternalAddress(const QString &host)
{
	g_config->extHost = host;
}

void AvCallManager::setAudioOutDevice(const QString &id)
{
	g_config->audioOutDeviceId = id;
}

void AvCallManager::setAudioInDevice(const QString &id)
{
	g_config->audioInDeviceId = id;
}

void AvCallManager::setVideoInDevice(const QString &id)
{
	g_config->videoInDeviceId = id;
}

#include "avcall.moc"
