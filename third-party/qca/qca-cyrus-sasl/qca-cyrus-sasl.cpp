/*
 * qca-sasl.cpp - SASL plugin for QCA
 * Copyright (C) 2003-2007  Justin Karneges <justin@affinix.com>
 * Copyright (C) 2006  Michail Pishchagin <mblsha@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include <QtCrypto>
#include <qcaprovider.h>
#include <QDebug>
#include <QtPlugin>

extern "C"
{
#include <sasl/sasl.h>
}

#include <QStringList>
#include <QList>
#include <QFile>

#define SASL_BUFSIZE 8192
#define SASL_APP     "qca"

using namespace QCA;

namespace saslQCAPlugin {

class saslProvider : public Provider
{
public:
	saslProvider();
	void init();
	~saslProvider();
	int qcaVersion() const;
	QString name() const;
	QString credit() const;
	QStringList features() const;
	Context *createContext(const QString &type);

	bool client_init;
	bool server_init;
	QString appname;
};

//----------------------------------------------------------------------------
// SASLParams
//----------------------------------------------------------------------------

class SASLParams
{
public:
	class SParams
	{
	public:
		bool user, authzid, pass, realm;
	};

	SASLParams()
	{
		reset();
	}

	void reset()
	{
		resetNeed();
		resetHave();
		foreach(char *result, results)
			delete result;
		results.clear();
	}

	void resetNeed()
	{
		need.user = false;
		need.authzid = false;
		need.pass = false;
		need.realm = false;
	}

	void resetHave()
	{
		have.user = false;
		have.authzid = false;
		have.pass = false;
		have.realm = false;
	}

	void setUsername(const QString &s)
	{
		have.user = true;
		user = s;
	}

	void setAuthzid(const QString &s)
	{
		have.authzid = true;
		authzid = s;
	}

	void setPassword(const SecureArray &s)
	{
		have.pass = true;
		pass = QString::fromUtf8(s.toByteArray());
	}

	void setRealm(const QString &s)
	{
		have.realm = true;
		realm = s;
	}

	void applyInteract(sasl_interact_t *needp)
	{
		for(int n = 0; needp[n].id != SASL_CB_LIST_END; ++n) {
			if(needp[n].id == SASL_CB_AUTHNAME)
				need.user = true;       // yes, I know these
			if(needp[n].id == SASL_CB_USER)
				need.authzid = true;    // look backwards
			if(needp[n].id == SASL_CB_PASS)
				need.pass = true;
			if(needp[n].id == SASL_CB_GETREALM)
				need.realm = true;
		}
	}

	void extractHave(sasl_interact_t *needp)
	{
		for(int n = 0; needp[n].id != SASL_CB_LIST_END; ++n) {
			if(needp[n].id == SASL_CB_AUTHNAME && have.user)
				setValue(&needp[n], user);
			if(needp[n].id == SASL_CB_USER && have.authzid)
				setValue(&needp[n], authzid);
			if(needp[n].id == SASL_CB_PASS && have.pass)
				setValue(&needp[n], pass);
			if(needp[n].id == SASL_CB_GETREALM && have.realm)
				setValue(&needp[n], realm);
		}
	}

	bool missingAny() const
	{
		if((need.user && !have.user) /*|| (need.authzid && !have.authzid)*/ || (need.pass && !have.pass) /*|| (need.realm && !have.realm)*/)
			return true;
		return false;
	}

	SParams missing() const
	{
		SParams np = need;
		if(have.user)
			np.user = false;
		if(have.authzid)
			np.authzid = false;
		if(have.pass)
			np.pass = false;
		if(have.realm)
			np.realm = false;
		return np;
	}

	void setValue(sasl_interact_t *i, const QString &s)
	{
		if(i->result)
			return;
		QByteArray cs = s.toUtf8();
		int len = cs.length();
		char *p = new char[len+1];
		memcpy(p, cs.data(), len);
		p[len] = 0;
		i->result = p;
		i->len = len;

		// record this
		results.append(p);
	}

	QList<char *> results;
	SParams need;
	SParams have;
	QString user, authzid, pass, realm;
};

static QByteArray makeByteArray(const void *in, unsigned int len)
{
	QByteArray buf(len, 0);
	memcpy(buf.data(), in, len);
	return buf;
}

static QString addrString(const SASLContext::HostPort &hp)
{
	return (hp.addr + ';' + QString::number(hp.port));
}

//----------------------------------------------------------------------------
// saslContext
//----------------------------------------------------------------------------

class saslContext : public SASLContext
{
	saslProvider *g;

	// core props
	QString service, host;
	QString localAddr, remoteAddr;

	// security props
	int secflags;
	int ssf_min, ssf_max;
	QString ext_authid;
	int ext_ssf;

	sasl_conn_t *con;
	sasl_interact_t *need;
	int maxoutbuf;
	sasl_callback_t *callbacks;

	// state
	bool servermode;
	int step;
	bool in_sendFirst;
	QByteArray in_buf;
	QString in_mech;
	bool in_useClientInit;
	QByteArray in_clientInit;
	QString out_mech;
	// bool out_useClientInit;
	// QByteArray out_clientInit;
	QByteArray out_buf;

	SASLParams params;
	QString sc_username, sc_authzid;
	bool ca_flag, ca_done, ca_skip;
	int last_r;

	int result_ssf;
	Result result_result;
	bool result_haveClientInit;
	QStringList result_mechlist;
	SASL::AuthCondition result_authCondition;
	QByteArray result_to_net;
	QByteArray result_plain;
	int result_encoded;

private:
	void resetState()
	{
		if(con) {
			sasl_dispose(&con);
			con = 0;
		}
		need = 0;
		if(callbacks) {
			delete callbacks;
			callbacks = 0;
		}

		localAddr = "";
		remoteAddr = "";
		maxoutbuf = 128;
		sc_username = "";
		sc_authzid = "";

		result_authCondition = SASL::AuthFail;
		result_haveClientInit = false;
		result_mechlist.clear();
		result_plain.clear();
		result_plain.clear();
		result_plain.clear();
		result_ssf = 0;
	}

	void resetParams()
	{
		params.reset();
		secflags = 0;
		ssf_min = 0;
		ssf_max = 0;
		ext_authid = "";
		ext_ssf = 0;
	}

	bool setsecprops()
	{
		sasl_security_properties_t secprops;
		secprops.min_ssf = ssf_min;
		secprops.max_ssf = ssf_max;
		secprops.maxbufsize = SASL_BUFSIZE;
		secprops.property_names = NULL;
		secprops.property_values = NULL;
		secprops.security_flags = secflags;
		int r = sasl_setprop(con, SASL_SEC_PROPS, &secprops);
		if(r != SASL_OK)
			return false;

		if(!ext_authid.isEmpty()) {
			const char *authid = ext_authid.toLatin1().data();
			sasl_ssf_t ssf = ext_ssf;
			r = sasl_setprop(con, SASL_SSF_EXTERNAL, &ssf);
			if(r != SASL_OK)
				return false;
			r = sasl_setprop(con, SASL_AUTH_EXTERNAL, &authid);
			if(r != SASL_OK)
				return false;
		}

		return true;
	}

	void setAuthCondition(int r)
	{
            //qDebug() << "authcondition: " << r;
		SASL::AuthCondition x;
		switch(r) {
			// common
			case SASL_NOMECH:    x = SASL::NoMechanism; break;
			case SASL_BADPROT:   x = SASL::BadProtocol; break;

			// client
			case SASL_BADSERV:   x = SASL::BadServer; break;

			// server
			case SASL_BADAUTH:   x = SASL::BadAuth; break;
			case SASL_NOAUTHZ:   x = SASL::NoAuthzid; break;
			case SASL_TOOWEAK:   x = SASL::TooWeak; break;
			case SASL_ENCRYPT:   x = SASL::NeedEncrypt; break;
			case SASL_EXPIRED:   x = SASL::Expired; break;
			case SASL_DISABLED:  x = SASL::Disabled; break;
			case SASL_NOUSER:    x = SASL::NoUser; break;
			case SASL_UNAVAIL:   x = SASL::RemoteUnavailable; break;

			default: x = SASL::AuthFail; break;
		}
		result_authCondition = x;
	}

	void getssfparams()
	{
		const void *maybe_sff;
		if( SASL_OK == sasl_getprop( con, SASL_SSF, &maybe_sff ) )
			result_ssf = *(const int*)maybe_sff;

		const void *maybe_maxoutbuf;
		if (SASL_OK == sasl_getprop( con, SASL_MAXOUTBUF, &maybe_maxoutbuf ) )
			maxoutbuf = *(const int*)maybe_maxoutbuf;
	}

	static int scb_checkauth(sasl_conn_t *, void *context, const char *requested_user, unsigned, const char *auth_identity, unsigned, const char *, unsigned, struct propctx *)
	{
		saslContext *that = (saslContext *)context;
		that->sc_username = auth_identity; // yeah yeah, it looks
		that->sc_authzid = requested_user; // backwards, but it is right
		that->ca_flag = true;
		return SASL_OK;
	}

	void clientTryAgain()
	{
		result_haveClientInit = false;

		if(step == 0) {
			const char *clientout, *m;
			unsigned int clientoutlen;

			need = 0;
			QString list = result_mechlist.join(" ");
			int r;
			while(1) {
				if(need)
					params.extractHave(need);
				if(in_sendFirst)
					r = sasl_client_start(con, list.toLatin1().data(), &need, &clientout, &clientoutlen, &m);
				else
					r = sasl_client_start(con, list.toLatin1().data(), &need, NULL, NULL, &m);
				if(r != SASL_INTERACT)
					break;

				params.applyInteract(need);
				if(params.missingAny()) {
					out_mech = m;
					result_result = Params;
					return;
				}
			}
			if(r != SASL_OK && r != SASL_CONTINUE) {
				setAuthCondition(r);
				result_result = Error;
				return;
			}

			out_mech = m;
			if(in_sendFirst && clientout) {
				out_buf = makeByteArray(clientout, clientoutlen);
				result_haveClientInit = true;
			}

			++step;

			if(r == SASL_OK) {
				getssfparams();
				result_result = Success;
				return;
			}
			result_result = Continue;
			return;
		}
		else {
			const char *clientout;
			unsigned int clientoutlen;
			int r;
			while(1) {
				if(need)
					params.extractHave(need);
				//printf("sasl_client_step(con, {%s}, %d, &need, &clientout, &clientoutlen);\n", in_buf.data(), in_buf.size());
				r = sasl_client_step(con, in_buf.data(), in_buf.size(), &need, &clientout, &clientoutlen);
				//printf("returned: %d\n", r);
				if(r != SASL_INTERACT)
					break;

				params.applyInteract(need);
				if(params.missingAny()) {
					result_result = Params;
					return;
				}
			}
			if(r != SASL_OK && r != SASL_CONTINUE) {
				setAuthCondition(r);
				result_result = Error;
				return;
			}
			out_buf = makeByteArray(clientout, clientoutlen);
			if(r == SASL_OK) {
				getssfparams();
				result_result = Success;
				return;
			}
			result_result = Continue;
			return;
		}
	}

	void serverTryAgain()
	{
		if(step == 0) {
			if(!ca_skip) {
				const char *clientin = 0;
				unsigned int clientinlen = 0;
				if(in_useClientInit) {
					clientin = in_clientInit.data();
					clientinlen = in_clientInit.size();
				}
				const char *serverout;
				unsigned int serveroutlen;
				ca_flag = false;
				int r = sasl_server_start(con, in_mech.toLatin1().data(), clientin, clientinlen, &serverout, &serveroutlen);
				if(r != SASL_OK && r != SASL_CONTINUE) {
					setAuthCondition(r);
					result_result = Error;
					return;
				}
				out_buf = makeByteArray(serverout, serveroutlen);
				last_r = r;
				if(ca_flag && !ca_done) {
					ca_done = true;
					ca_skip = true;
					result_result = AuthCheck;
					return;
				}
			}
			ca_skip = false;
			++step;

			if(last_r == SASL_OK) {
				getssfparams();
				result_result = Success;
				return;
			}
			result_result = Continue;
			return;
		}
		else {
			if(!ca_skip) {
				const char *serverout;
				unsigned int serveroutlen;
				int r = sasl_server_step(con, in_buf.data(), in_buf.size(), &serverout, &serveroutlen);
				if(r != SASL_OK && r != SASL_CONTINUE) {
					setAuthCondition(r);
					result_result = Error;
					return;
				}
				if(r == SASL_OK)
					out_buf.resize(0);
				else
					out_buf = makeByteArray(serverout, serveroutlen);
				last_r = r;
				if(ca_flag && !ca_done) {
					ca_done = true;
					ca_skip = true;
					result_result = AuthCheck;
					return;
				}
			}
			ca_skip = false;
			if(last_r == SASL_OK) {
				getssfparams();
				result_result = Success;
				return;
			}
			result_result = Continue;
			return;
		}
	}

	bool sasl_endecode(const QByteArray &in, QByteArray *out, bool enc)
	{
		// no security
		if(result_ssf == 0) {
			*out = in;
			return true;
		}

		int at = 0;
		out->resize(0);
		while(1) {
			int size = in.size() - at;
			if(size == 0)
				break;
			if(size > maxoutbuf)
				size = maxoutbuf;
			const char *outbuf;
			unsigned len;
			int r;
			if(enc)
				r = sasl_encode(con, in.data() + at, size, &outbuf, &len);
			else
				r = sasl_decode(con, in.data() + at, size, &outbuf, &len);
			if(r != SASL_OK)
				return false;
			int oldsize = out->size();
			out->resize(oldsize + len);
			memcpy(out->data() + oldsize, outbuf, len);
			at += size;
		}
		return true;
	}

	void doResultsReady()
	{
		QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
	}

public:
	saslContext(saslProvider *_g)
	: SASLContext(_g)
	{
		result_result = Success;
		g = _g;
		con = 0;
		callbacks = 0;

		reset();
	}

	~saslContext()
	{
		reset();
	}

	virtual Provider::Context *clone() const
	{
		return 0;
	}

	virtual Result result() const
	{
		return result_result;
	}

	virtual void reset()
	{
		resetState();
		resetParams();
	}

	virtual void setup(const QString &_service, const QString &_host, const HostPort *local, const HostPort *remote, const QString &ext_id, int _ext_ssf)
	{
		service = _service;
		host = _host;
		localAddr = local ? addrString(*local) : "";
		remoteAddr = remote ? addrString(*remote) : "";
		ext_authid = ext_id;
		ext_ssf = _ext_ssf;
	}

	virtual int ssf() const
	{
		return result_ssf;
	}

	virtual void startClient(const QStringList &mechlist, bool allowClientSendFirst)
	{
		resetState();

		in_sendFirst = allowClientSendFirst;

		if(!g->client_init) {
			sasl_client_init(NULL);
			g->client_init = true;
		}

		callbacks = new sasl_callback_t[5];

		callbacks[0].id = SASL_CB_GETREALM;
		callbacks[0].proc = 0;
		callbacks[0].context = 0;

		callbacks[1].id = SASL_CB_USER;
		callbacks[1].proc = 0;
		callbacks[1].context = 0;

		callbacks[2].id = SASL_CB_AUTHNAME;
		callbacks[2].proc = 0;
		callbacks[2].context = 0;

		callbacks[3].id = SASL_CB_PASS;
		callbacks[3].proc = 0;
		callbacks[3].context = 0;

		callbacks[4].id = SASL_CB_LIST_END;
		callbacks[4].proc = 0;
		callbacks[4].context = 0;

		result_result = Error;

		int r = sasl_client_new(service.toLatin1().data(), host.toLatin1().data(), localAddr.isEmpty() ? 0 : localAddr.toLatin1().data(), remoteAddr.isEmpty() ? 0 : remoteAddr.toLatin1().data(), callbacks, 0, &con);
		if(r != SASL_OK) {
			setAuthCondition(r);
			doResultsReady();
			return;
		}

		if(!setsecprops())
		{
			doResultsReady();
			return;
		}

		result_mechlist = mechlist;
		servermode = false;
		step = 0;
		result_result = Success;
		clientTryAgain();
		doResultsReady();
		return;
	}

	// TODO: make use of disableServerSendLast
	virtual void startServer(const QString &realm, bool disableServerSendLast)
	{
		Q_UNUSED(disableServerSendLast);
		resetState();

		g->appname = SASL_APP;
		if(!g->server_init) {
			sasl_server_init(NULL, QFile::encodeName(g->appname));
			g->server_init = true;
		}

		callbacks = new sasl_callback_t[2];

		callbacks[0].id = SASL_CB_PROXY_POLICY;
		callbacks[0].proc = (int(*)())scb_checkauth;
		callbacks[0].context = this;

		callbacks[1].id = SASL_CB_LIST_END;
		callbacks[1].proc = 0;
		callbacks[1].context = 0;

		result_result = Error;

		int r = sasl_server_new(service.toLatin1().data(), host.toLatin1().data(), !realm.isEmpty() ? realm.toLatin1().data() : 0, localAddr.isEmpty() ? 0 : localAddr.toLatin1().data(), remoteAddr.isEmpty() ? 0 : remoteAddr.toLatin1().data(), callbacks, 0, &con);
		if(r != SASL_OK) {
			setAuthCondition(r);
			doResultsReady();
			return;
		}

		if(!setsecprops())
		{
			doResultsReady();
			return;
		}

		const char *ml;
		r = sasl_listmech(con, 0, 0, " ", 0, &ml, 0, 0);
		if(r != SASL_OK)
			return;
		result_mechlist = QString::fromUtf8(ml).split(' ');

		servermode = true;
		step = 0;
		ca_done = false;
		ca_skip = false;
		result_result = Success;
		doResultsReady();
		return;
	}

	virtual void serverFirstStep(const QString &mech, const QByteArray *clientInit)
	{
		in_mech = mech;
		if(clientInit) {
			in_useClientInit = true;
			in_clientInit = *clientInit;
		}
		else
			in_useClientInit = false;
		serverTryAgain();
		doResultsReady();
	}

	virtual SASL::Params clientParams() const
	{
		SASLParams::SParams sparams = params.missing();
		return SASL::Params(sparams.user, sparams.authzid, sparams.pass, sparams.realm);
	}

	virtual void setClientParams(const QString *user, const QString *authzid, const SecureArray *pass, const QString *realm)
	{
		if(user)
			params.setUsername(*user);
		if(authzid)
			params.setAuthzid(*authzid);
		if(pass)
			params.setPassword(*pass);
		if(realm)
			params.setRealm(*realm);
	}

	virtual QString username() const
	{
		return sc_username;
	}

	virtual QString authzid() const
	{
		return sc_authzid;
	}

	virtual void nextStep(const QByteArray &from_net)
	{
		in_buf = from_net;
		tryAgain();
	}

	virtual void tryAgain()
	{
		if(servermode)
			serverTryAgain();
		else
			clientTryAgain();
		doResultsReady();
	}

	virtual QString mech() const
	{
		if (servermode)
			return in_mech;
		else
			return out_mech;
	}

	virtual QStringList mechlist() const
	{
		return result_mechlist;
	}

	virtual QStringList realmlist() const
	{
		// TODO
		return QStringList();
	}

	virtual void setConstraints(SASL::AuthFlags f, int minSSF, int maxSSF)
	{
		int sf = 0;
		if( !(f & SASL::AllowPlain) )
			sf |= SASL_SEC_NOPLAINTEXT;
		// if( !(f & SASL::AllowActiveVulnerable) ) // TODO
		// 	sf |= SASL_SEC_NOACTIVE;
		// if( !(f & SASL::AllowDictVulnerable) ) // TODO
		// 	sf |= SASL_SEC_NODICTIONARY;
		if( !(f & SASL::AllowAnonymous) )
			sf |= SASL_SEC_NOANONYMOUS;
		if( f & SASL::RequireForwardSecrecy )
			sf |= SASL_SEC_FORWARD_SECRECY;
		if( f & SASL::RequirePassCredentials )
			sf |= SASL_SEC_PASS_CREDENTIALS;
		if( f & SASL::RequireMutualAuth )
			sf |= SASL_SEC_MUTUAL_AUTH;

		secflags = sf;
		ssf_min = minSSF;
		ssf_max = maxSSF;
	}

	virtual bool waitForResultsReady(int msecs)
	{
		// TODO: for now, all operations block anyway
		Q_UNUSED(msecs);
		return true;
	}

	virtual void update(const QByteArray &from_net, const QByteArray &from_app)
	{
		bool ok = true;
		if(!from_app.isEmpty())
			ok = sasl_endecode(from_app, &result_to_net, true);
		if(ok && !from_net.isEmpty())
			ok = sasl_endecode(from_net, &result_plain, false);
		result_result = ok ? Success : Error;
		result_encoded = from_app.size();

		//printf("update (from_net=%d, to_net=%d, from_app=%d, to_app=%d)\n", from_net.size(), result_to_net.size(), from_app.size(), result_plain.size());

		doResultsReady();
	}

	virtual bool haveClientInit() const
	{
		return result_haveClientInit;
	}

	virtual QByteArray stepData() const
	{
		return out_buf;
	}

	virtual QByteArray to_net()
	{
		QByteArray a = result_to_net;
		result_to_net.clear();
		return a;
	}

	virtual int encoded() const
	{
		return result_encoded;
	}

	virtual QByteArray to_app()
	{
		QByteArray a = result_plain;
		result_plain.clear();
		return a;
	}

	virtual SASL::AuthCondition authCondition() const
	{
		return result_authCondition;
	}
};

//----------------------------------------------------------------------------
// saslProvider
//----------------------------------------------------------------------------
saslProvider::saslProvider()
{
	client_init = false;
	server_init = false;
}

void saslProvider::init()
{
}

saslProvider::~saslProvider()
{
	if(client_init || server_init)
		sasl_done();
}

int saslProvider::qcaVersion() const
{
        return QCA_VERSION;
}

QString saslProvider::name() const
{
	return "qca-cyrus-sasl";
}

QString saslProvider::credit() const
{
	return QString(); // TODO
}

QStringList saslProvider::features() const
{
	QStringList list;
	list += "sasl";

	return list;
}

Provider::Context *saslProvider::createContext(const QString &type)
{
	if ( type == "sasl" )
		return new saslContext( this );

	return 0;
}

} // namespace saslQCAPlugin

using namespace saslQCAPlugin;

//----------------------------------------------------------------------------
// saslPlugin
//----------------------------------------------------------------------------

class saslPlugin : public QObject, public QCAPlugin
{
	Q_OBJECT
	Q_INTERFACES(QCAPlugin)
public:
	virtual Provider *createProvider() { return new saslProvider; }
};

#include "qca-cyrus-sasl.moc"

Q_EXPORT_PLUGIN2(qca_cyrus_sasl, saslPlugin)

