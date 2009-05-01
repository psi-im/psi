#ifndef JINGLERTP_H
#define JINGLERTP_H

#include <QObject>

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

class JingleRtpSessionPrivate;
class JingleRtpManagerPrivate;

class JingleRtpSession : public QObject
{
	Q_OBJECT

public:
	enum Mode
	{
		Audio,
		Video,
		Both
	};

	JingleRtpSession(const JingleRtpSession &from);
	~JingleRtpSession();

	XMPP::Jid jid() const;
	Mode mode() const;

	void connectToJid(const XMPP::Jid &jid, Mode mode, int kbps = -1);
	void accept(Mode mode, int kbps = -1);
	void reject();

	void setIncomingVideo(PsiMedia::VideoWidget *widget);

signals:
	void rejected();
	void activated();

private:
	friend class JingleRtpSessionPrivate;
	friend class JingleRtpManager;
	friend class JingleRtpManagerPrivate;
	JingleRtpSession();

	JingleRtpSessionPrivate *d;
};

class JingleRtpManager : public QObject
{
	Q_OBJECT

public:
	JingleRtpManager(PsiAccount *pa);
	~JingleRtpManager();

	JingleRtpSession *createOutgoing();
	JingleRtpSession *takeIncoming();

	static void config();
	static bool isSupported();
	static bool isVideoSupported();

	void setSelfAddress(const QHostAddress &addr);
	void setStunHost(const QString &host, int port);

	static void setBasePort(int port);
	static void setExternalAddress(const QString &host);
	static void setAudioOutDevice(const QString &id);
	static void setAudioInDevice(const QString &id);
	static void setVideoInDevice(const QString &id);

signals:
	void incomingReady();

private:
	friend class JingleRtpManagerPrivate;
	friend class JingleRtpSession;
	friend class JingleRtpSessionPrivate;

	JingleRtpManagerPrivate *d;
};

#endif
