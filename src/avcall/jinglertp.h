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
	~JingleRtpSession();

	XMPP::Jid jid() const;
	void connectToJid(const XMPP::Jid &jid);
	void accept();
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

	//static JingleRtpManager *instance();

	JingleRtpSession *createOutgoing();
	JingleRtpSession *takeIncoming();
	static void config();
	void setSelfAddress(const QHostAddress &addr);

signals:
	void incomingReady();

private:
	friend class JingleRtpManagerPrivate;
	friend class JingleRtpSession;
	friend class JingleRtpSessionPrivate;

	JingleRtpManagerPrivate *d;
};

#endif
