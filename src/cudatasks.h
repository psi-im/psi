#include "im.h"

namespace XMPP {

class JT_CudaLogin : public Task
{
	Q_OBJECT
public:
	JT_CudaLogin(Task *);

	void get(const Jid &, const QString &target = QString());
	void onGo();
	bool take(const QDomElement &);

	const Jid &jid() const;
	const QString &url() const;

	bool supportsTarget() const;

private:
	QDomElement iq;
	Jid j;
	QString v_url;
	bool _supportsTarget;
};

class JT_GetDisclaimer : public Task
{
	Q_OBJECT
public:
	JT_GetDisclaimer(Task *);

	void get(const Jid &);
	void onGo();
	bool take(const QDomElement &);

	const Jid & jid() const;
	const QString & message() const;

private:
	QDomElement iq;

	Jid j;
	QString v_msg;
};

class JT_GetClientVersion : public Task
{
	Q_OBJECT
public:
	JT_GetClientVersion(Task *);

	void get(const Jid &);
	void onGo();
	bool take(const QDomElement &);

	const Jid & jid() const;
	const QString & name() const;
	const QString & version() const;
	const QString & updater() const;
	const QString & ssl() const;
	const QString & message() const;
	const QString & message_title() const;
	const QString & port() const;
	const QString & os() const;

private:
	QDomElement iq;

	Jid j;
	QString v_name, v_ver, v_os, v_updater, v_ssl, v_message, v_message_title, v_port;
};

class JT_PushGetClientVersion : public Task
{
	Q_OBJECT
public:
	class Url
	{
	public:
		QString type;
		QString url;
	};

	JT_PushGetClientVersion(Task *);

	bool take(const QDomElement &);

signals:
	void newVersion(const QString &ver, const QList<JT_PushGetClientVersion::Url> &urls);
};

}
