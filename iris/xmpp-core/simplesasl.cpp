/*
 * simplesasl.cpp - Simple SASL implementation
 * Copyright (C) 2003  Justin Karneges
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "simplesasl.h"

#include <qhostaddress.h>
#include <qstringlist.h>
#include <q3ptrlist.h>
#include <QList>
#include <qca.h>
#include <Q3CString>
#include <stdlib.h>
#include <QtCrypto>
#include <QDebug>

#define SIMPLESASL_PLAIN

namespace XMPP {
struct Prop
{
	Q3CString var, val;
};

class PropList : public QList<Prop>
{
public:
	PropList() : QList<Prop>()
	{
	}

	void set(const Q3CString &var, const Q3CString &val)
	{
		Prop p;
		p.var = var;
		p.val = val;
		append(p);
	}

	Q3CString get(const Q3CString &var)
	{
		for(ConstIterator it = begin(); it != end(); ++it) {
			if((*it).var == var)
				return (*it).val;
		}
		return Q3CString();
	}

	Q3CString toString() const
	{
		Q3CString str;
		bool first = true;
		for(ConstIterator it = begin(); it != end(); ++it) {
			if(!first)
				str += ',';
			if ((*it).var == "realm" || (*it).var == "nonce" || (*it).var == "username" || (*it).var == "cnonce" || (*it).var == "digest-uri" || (*it).var == "authzid")
				str += (*it).var + "=\"" + (*it).val + '\"';
			else 
				str += (*it).var + "=" + (*it).val;
			first = false;
		}
		return str;
	}

	bool fromString(const QByteArray &str)
	{
		PropList list;
		int at = 0;
		while(1) {
			while (at < str.length() && (str[at] == ',' || str[at] == ' ' || str[at] == '\t'))
				  ++at;
			int n = str.find('=', at);
			if(n == -1)
				break;
			Q3CString var, val;
			var = str.mid(at, n-at);
			at = n + 1;
			if(str[at] == '\"') {
				++at;
				n = str.find('\"', at);
				if(n == -1)
					break;
				val = str.mid(at, n-at);
				at = n + 1;
			}
			else {
				n = at;
				while (n < str.length() && str[n] != ',' && str[n] != ' ' && str[n] != '\t')
					++n;
				val = str.mid(at, n-at);
				at = n;
			}
			Prop prop;
			prop.var = var;
			if (var == "qop" || var == "cipher") {
				int a = 0;
				while (a < val.length()) {
					while (a < val.length() && (val[a] == ',' || val[a] == ' ' || val[a] == '\t'))
						++a;
					if (a == val.length())
						break;
					n = a+1;
					while (n < val.length() && val[n] != ',' && val[n] != ' ' && val[n] != '\t')
						++n;
					prop.val = val.mid(a, n-a);
					list.append(prop);
					a = n+1;
				}
			}
			else {
				prop.val = val;
				list.append(prop);
			}

			if(at >= str.size() - 1 || (str[at] != ',' && str[at] != ' ' && str[at] != '\t'))
				break;
		}

		// integrity check
		if(list.varCount("nonce") != 1)
			return false;
		if(list.varCount("algorithm") != 1)
			return false;
		*this = list;
		return true;
	}

	int varCount(const Q3CString &var)
	{
		int n = 0;
		for(ConstIterator it = begin(); it != end(); ++it) {
			if((*it).var == var)
				++n;
		}
		return n;
	}

	QStringList getValues(const Q3CString &var)
	{
		QStringList list;
		for(ConstIterator it = begin(); it != end(); ++it) {
			if((*it).var == var)
				list += (*it).val;
		}
		return list;
	}
};

class SimpleSASLContext : public QCA::SASLContext
{
public:
		class ParamsMutable
		{
		public:
			/**
			   User is held
			*/
			bool user;

			/**
			   Authorization ID is held
			*/
			bool authzid;

			/**
			   Password is held
			*/
			bool pass;

			/**
			   Realm is held
			*/
			bool realm;
		};
	// core props
	QString service, host;

	// state
	int step;
	bool capable;
	bool allow_plain;
	QByteArray out_buf, in_buf;
	QString mechanism_;
	QString out_mech;

	ParamsMutable need;
	ParamsMutable have;
	QString user, authz, realm;
	QCA::SecureArray pass;
	Result result_;
	QCA::SASL::AuthCondition authCondition_;
	QByteArray result_to_net_, result_to_app_;
	int encoded_;

	SimpleSASLContext(QCA::Provider* p) : QCA::SASLContext(p)
	{
		reset();
	}

	~SimpleSASLContext()
	{
		reset();
	}

	void reset()
	{
		resetState();

		capable = true;
		allow_plain = false;
		need.user = false;
		need.authzid = false;
		need.pass = false;
		need.realm = false;
		have.user = false;
		have.authzid = false;
		have.pass = false;
		have.realm = false;
		user = QString();
		authz = QString();
		pass = QCA::SecureArray();
		realm = QString();
	}

	void resetState()
	{
		out_mech = QString();
		out_buf.resize(0);
		authCondition_ = QCA::SASL::AuthFail;
	}

	virtual void setConstraints(QCA::SASL::AuthFlags flags, int ssfMin, int) {
		if(flags & (QCA::SASL::RequireForwardSecrecy | QCA::SASL::RequirePassCredentials | QCA::SASL::RequireMutualAuth) || ssfMin > 0)
			capable = false;
		else
			capable = true;
		allow_plain = flags & QCA::SASL::AllowPlain;
	}
	
	virtual void setup(const QString& _service, const QString& _host, const QCA::SASLContext::HostPort*, const QCA::SASLContext::HostPort*, const QString&, int) {
		service = _service;
		host = _host;
	}
	
	virtual void startClient(const QStringList &mechlist, bool allowClientSendFirst) {
		Q_UNUSED(allowClientSendFirst);

		mechanism_ = QString();
		foreach(QString mech, mechlist) {
			if (mech == "DIGEST-MD5") {
				mechanism_ = "DIGEST-MD5";
				break;
			}
#ifdef SIMPLESASL_PLAIN
			if (mech == "PLAIN" && allow_plain) 
				mechanism_ = "PLAIN";
#endif
		}

		if(!capable || mechanism_.isEmpty()) {
			result_ = Error;
			authCondition_ = QCA::SASL::NoMechanism;
			if (!capable)
				qWarning("simplesasl.cpp: Not enough capabilities");
			if (mechanism_.isEmpty()) 
				qWarning("simplesasl.cpp: No mechanism available");
			QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
			return;
		}

		resetState();
		result_ = Continue;
		step = 0;
		tryAgain();
	}

	virtual void nextStep(const QByteArray &from_net) {
		in_buf = from_net;
		tryAgain();
	}

	virtual void tryAgain() {
		// All exits of the method must emit the ready signal
		// so all exits go through a goto ready; 
		if(step == 0) {
			out_mech = mechanism_;
			
#ifdef SIMPLESASL_PLAIN
			// PLAIN 
			if (out_mech == "PLAIN") {
				// First, check if we have everything
				if(need.user || need.pass) {
					qWarning("simplesasl.cpp: Did not receive necessary auth parameters");
					result_ = Error;
					goto ready;
				}
				if(!have.user)
					need.user = true;
				if(!have.pass)
					need.pass = true;
				if(need.user || need.pass) {
					result_ = Params;
					goto ready;
				}

				// Continue with authentication
				QByteArray plain;
				if (!authz.isEmpty())
					plain += authz.utf8();
			   	plain += '\0' + user.toUtf8() + '\0' + pass.toByteArray();
				out_buf.resize(plain.length());
				memcpy(out_buf.data(), plain.data(), out_buf.size());
			}
#endif
			++step;
			if (out_mech == "PLAIN")
				result_ = Success;
			else
				result_ = Continue;
		}
		else if(step == 1) {
			// if we still need params, then the app has failed us!
			if(need.user || need.authzid || need.pass || need.realm) {
				qWarning("simplesasl.cpp: Did not receive necessary auth parameters");
				result_ = Error;
				goto ready;
			}

			// see if some params are needed
			if(!have.user)
				need.user = true;
			//if(!have.authzid)
			//	need.authzid = true;
			if(!have.pass)
				need.pass = true;
			if(need.user || need.authzid || need.pass) {
				result_ = Params;
				goto ready;
			}

			// get props
			QByteArray cs(in_buf);
			PropList in;
			if(!in.fromString(cs)) {
				authCondition_ = QCA::SASL::BadProtocol;
				result_ = Error;
				goto ready;
			}
			//qDebug() << (QString("simplesasl.cpp: IN: %1").arg(QString(in.toString())));

			// make a cnonce
			QByteArray a(32);
			for(int n = 0; n < (int)a.size(); ++n)
				a[n] = (char)(256.0*rand()/(RAND_MAX+1.0));
			Q3CString cnonce = QCA::Base64().arrayToString(a).latin1();

			// make other variables
			if (realm.isEmpty())
				realm = QString::fromUtf8(in.get("realm"));
			Q3CString nonce = in.get("nonce");
			Q3CString nc = "00000001";
			Q3CString uri = service.utf8() + '/' + host.utf8();
			Q3CString qop = "auth";

			// build 'response'
			Q3CString X = user.utf8() + ':' + realm.utf8() + ':' + Q3CString(pass.toByteArray());
			QByteArray Y = QCA::Hash("md5").hash(X).toByteArray();
			QByteArray tmp = ':' + nonce + ':' + cnonce;
			if (!authz.isEmpty())
				tmp += ':' + authz.utf8();
			//qDebug() << (QString(tmp));

			QByteArray A1(Y + tmp);
			QByteArray A2 = QByteArray("AUTHENTICATE:") + uri;
			Q3CString HA1 = QCA::Hash("md5").hashToString(A1).latin1();
			Q3CString HA2 = QCA::Hash("md5").hashToString(A2).latin1();
			Q3CString KD = HA1 + ':' + nonce + ':' + nc + ':' + cnonce + ':' + qop + ':' + HA2;
			Q3CString Z = QCA::Hash("md5").hashToString(KD).latin1();
			
			//qDebug() << (QString("simplesasl.cpp: A1 = %1").arg(QString(A1)).toAscii());
			//qDebug() << (QString("simplesasl.cpp: A2 = %1").arg(QString(A2)).toAscii());
			//qDebug() << (QString("simplesasl.cpp: KD = %1").arg(QString(KD)).toAscii());

			// build output
			PropList out;
			out.set("username", user.utf8());
			if (!realm.isEmpty())
				out.set("realm", realm.utf8());
			out.set("nonce", nonce);
			out.set("cnonce", cnonce);
			out.set("nc", nc);
			//out.set("serv-type", service.utf8());
			//out.set("host", host.utf8());
			out.set("digest-uri", uri);
			out.set("qop", qop);
			out.set("response", Z);
			out.set("charset", "utf-8");
			if (!authz.isEmpty())
				out.set("authzid", authz.utf8());
			QByteArray s(out.toString());
			//qDebug() << (QString("OUT: %1").arg(QString(out.toString())));

			// done
			out_buf.resize(s.length());
			memcpy(out_buf.data(), s.data(), out_buf.size());
			++step;
			result_ = Continue;
		}
		/*else if (step == 2) {
			//Commenting this out is Justin's fix for updated QCA.
			out_buf.resize(0);
			result_ = Continue;
			++step;
		}*/
		else {
			out_buf.resize(0);
			result_ = Success;
		}
ready:
		QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
	}

	virtual void update(const QByteArray &from_net, const QByteArray &from_app) {
		result_to_app_ = from_net;
		result_to_net_ = from_app;
		encoded_ = from_app.size();
		result_ = Success;
		QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
	}

	virtual bool waitForResultsReady(int msecs) {

		// TODO: for now, all operations block anyway
		Q_UNUSED(msecs);
		return true;
	}

	virtual Result result() const {
		return result_;
	}

	virtual QStringList mechlist() const {
		return QStringList();
	}
	
	virtual QString mech() const {
		return out_mech;
	}
	
	virtual bool haveClientInit() const {
		return out_mech == "PLAIN";
	}
	
	virtual QByteArray stepData() const {
		return out_buf;
	}
	
	virtual QByteArray to_net() {
		return result_to_net_;
	}
	
	virtual int encoded() const {
		return encoded_;
	}
	
	virtual QByteArray to_app() {
		return result_to_app_;
	}

	virtual int ssf() const {
		return 0;
	}

	virtual QCA::SASL::AuthCondition authCondition() const {
		return authCondition_;
	}

	virtual QCA::SASL::Params clientParams() const {
		return QCA::SASL::Params(need.user, need.authzid, need.pass, need.realm);
	}
	
	virtual void setClientParams(const QString *_user, const QString *_authzid, const QCA::SecureArray *_pass, const QString *_realm) {
		if(_user) {
			user = *_user;
			need.user = false;
			have.user = true;
		}
		if(_authzid) {
			authz = *_authzid;
			need.authzid = false;
			have.authzid = true;
		}
		if(_pass) {
			pass = *_pass;
			need.pass = false;
			have.pass = true;
		}
		if(_realm) {
			realm = *_realm;
			need.realm = false;
			have.realm = true;
		}
	}

	virtual QStringList realmlist() const
	{
		// TODO
		return QStringList();
	}

	virtual QString username() const {
		return QString();
	}

	virtual QString authzid() const {
		return QString();
	}

	virtual QCA::Provider::Context* clone() const {
		SimpleSASLContext* s = new SimpleSASLContext(provider());
		// TODO: Copy all the members
		return s;
	}
	
	virtual void startServer(const QString &, bool) {
		result_ =  QCA::SASLContext::Error;
		QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
	}
	virtual void serverFirstStep(const QString &, const QByteArray *) {
		result_ =  QCA::SASLContext::Error;
		QMetaObject::invokeMethod(this, "resultsReady", Qt::QueuedConnection);
	}

};

class QCASimpleSASL : public QCA::Provider
{
public:
	QCASimpleSASL() {}
	~QCASimpleSASL() {}

	void init()
	{
	}

	QString name() const {
		return "simplesasl";
	}

	QStringList features() const {
		return QStringList("sasl");
	}

	QCA::Provider::Context* createContext(const QString& cap)
	{
		if(cap == "sasl")
			return new SimpleSASLContext(this);
		return 0;
	}
	int qcaVersion() const
	{
		return QCA_VERSION;
	}
};

QCA::Provider *createProviderSimpleSASL()
{
	return (new QCASimpleSASL);
}

}
