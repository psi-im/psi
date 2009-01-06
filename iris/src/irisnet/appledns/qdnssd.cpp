/*
 * Copyright (C) 2007,2008  Justin Karneges
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include "qdnssd.h"

#include <QtCore>
#include <stdio.h>

// for ntohs
#ifdef Q_OS_WIN
# include <windows.h>
#else
# include <netinet/in.h>
#endif

#include "dns_sd.h"

namespace {

// safeobj stuff, from qca

void releaseAndDeleteLater(QObject *owner, QObject *obj)
{
	obj->disconnect(owner);
	obj->setParent(0);
	obj->deleteLater();
}

class SafeTimer : public QObject
{
	Q_OBJECT
public:
	SafeTimer(QObject *parent = 0) :
		QObject(parent)
	{
		t = new QTimer(this);
		connect(t, SIGNAL(timeout()), SIGNAL(timeout()));
	}

	~SafeTimer()
	{
		releaseAndDeleteLater(this, t);
	}

	int interval() const                { return t->interval(); }
	bool isActive() const               { return t->isActive(); }
	bool isSingleShot() const           { return t->isSingleShot(); }
	void setInterval(int msec)          { t->setInterval(msec); }
	void setSingleShot(bool singleShot) { t->setSingleShot(singleShot); }
	int timerId() const                 { return t->timerId(); }

public slots:
	void start(int msec)                { t->start(msec); }
	void start()                        { t->start(); }
	void stop()                         { t->stop(); }

signals:
	void timeout();

private:
	QTimer *t;
};

class SafeSocketNotifier : public QObject
{
	Q_OBJECT
public:
	SafeSocketNotifier(int socket, QSocketNotifier::Type type,
		QObject *parent = 0) :
		QObject(parent)
	{
		sn = new QSocketNotifier(socket, type, this);
		connect(sn, SIGNAL(activated(int)), SIGNAL(activated(int)));
	}

	~SafeSocketNotifier()
	{
		sn->setEnabled(false);
		releaseAndDeleteLater(this, sn);
	}

	bool isEnabled() const             { return sn->isEnabled(); }
	int socket() const                 { return sn->socket(); }
	QSocketNotifier::Type type() const { return sn->type(); }

public slots:
	void setEnabled(bool enable)       { sn->setEnabled(enable); }

signals:
	void activated(int socket);

private:
	QSocketNotifier *sn;
};

// DNSServiceRef must be allocated by the user and initialized by the
//   API.  Additionally, it is unclear from the API whether or not
//   DNSServiceRef can be copied (it is an opaque data structure).
//   What we'll do is allocate DNSServiceRef on the heap, allowing us
//   to maintain a pointer which /can/ be copied.  Also, we'll keep
//   a flag to indicate whether the allocated DNSServiceRef has been
//   initialized yet.
class ServiceRef
{
private:
	DNSServiceRef *_p;
	bool _initialized;

public:
	ServiceRef() :
		_initialized(false)
	{
		_p = (DNSServiceRef *)malloc(sizeof(DNSServiceRef));
	}

	~ServiceRef()
	{
		if(_initialized)
			DNSServiceRefDeallocate(*_p);
		free(_p);
	}

	DNSServiceRef *data()
	{
		return _p;
	}

	void setInitialized()
	{
		_initialized = true;
	}
};

class RecordRef
{
private:
	DNSRecordRef *_p;

public:
	RecordRef()
	{
		_p = (DNSRecordRef *)malloc(sizeof(DNSRecordRef));
	}

	~RecordRef()
	{
		free(_p);
	}

	DNSRecordRef *data()
	{
		return _p;
	}
};

class IdManager
{
private:
	QSet<int> set;
	int at;

	inline void bump_at()
	{
		if(at == 0x7fffffff)
			at = 0;
		else
			++at;
	}

public:
	IdManager() :
		at(0)
	{
	}

	int reserveId()
	{
		while(1)
		{
			if(!set.contains(at))
			{
				int id = at;
				set.insert(id);
				bump_at();
				return id;
			}

			bump_at();
		}
	}

	void releaseId(int id)
	{
		set.remove(id);
	}
};

}

//----------------------------------------------------------------------------
// QDnsSd
//----------------------------------------------------------------------------
class QDnsSd::Private : public QObject
{
	Q_OBJECT
public:
	QDnsSd *q;
	IdManager idManager;

	class SubRecord
	{
	public:
		Private *_self;
		int _id;
		RecordRef *_sdref;

		SubRecord(Private *self) :
			_self(self),
			_id(-1),
			_sdref(0)
		{
		}

		~SubRecord()
		{
			delete _sdref;
			_self->idManager.releaseId(_id);
		}
	};

	class Request
	{
	public:
		enum Type
		{
			Query,
			Browse,
			Resolve,
			Reg
		};

		Private *_self;
		int _type;
		int _id;
		ServiceRef *_sdref;
		int _sockfd;
		SafeSocketNotifier *_sn_read;
		SafeTimer *_errorTrigger;

		bool _doSignal;
		LowLevelError _lowLevelError;
		QList<QDnsSd::Record> _queryRecords;
		QList<QDnsSd::BrowseEntry> _browseEntries;
		QByteArray _resolveFullName;
		QByteArray _resolveHost;
		int _resolvePort;
		QByteArray _resolveTxtRecord;
		QByteArray _regDomain;
		bool _regConflict;

		QList<SubRecord*> _subRecords;

		Request(Private *self) :
			_self(self),
			_id(-1),
			_sdref(0),
			_sockfd(-1),
			_sn_read(0),
			_errorTrigger(0),
			_doSignal(false)
		{
		}

		~Request()
		{
			qDeleteAll(_subRecords);

			delete _errorTrigger;
			delete _sn_read;
			delete _sdref;
			_self->idManager.releaseId(_id);
		}

		int subRecordIndexById(int rec_id) const
		{
			for(int n = 0; n < _subRecords.count(); ++n)
			{
				if(_subRecords[n]->_id == rec_id)
					return n;
			}
			return -1;
		}
	};

	QHash<int,Request*> _requestsById;
	QHash<SafeSocketNotifier*,Request*> _requestsBySocket;
	QHash<SafeTimer*,Request*> _requestsByTimer;
	QHash<int,Request*> _requestsByRecId;

	Private(QDnsSd *_q) :
		QObject(_q),
		q(_q)
	{
	}

	~Private()
	{
		qDeleteAll(_requestsById);
	}

	void setDelayedError(Request *req, const LowLevelError &lowLevelError)
	{
		delete req->_sdref;
		req->_sdref = 0;

		req->_lowLevelError = lowLevelError;

		req->_errorTrigger = new SafeTimer(this);
		connect(req->_errorTrigger, SIGNAL(timeout()), SLOT(doError()));
		req->_errorTrigger->setSingleShot(true);

		_requestsByTimer.insert(req->_errorTrigger, req);

		req->_errorTrigger->start();
	}

	void removeRequest(Request *req)
	{
		foreach(const SubRecord *srec, req->_subRecords)
			_requestsByRecId.remove(srec->_id);
		if(req->_errorTrigger)
			_requestsByTimer.remove(req->_errorTrigger);
		if(req->_sn_read)
			_requestsBySocket.remove(req->_sn_read);
		_requestsById.remove(req->_id);
		delete req;
	}

	int regIdForRecId(int rec_id) const
	{
		Request *req = _requestsByRecId.value(rec_id);
		if(req)
			return req->_id;
		return -1;
	}

	int query(const QByteArray &name, int qType)
	{
		int id = idManager.reserveId();

		Request *req = new Request(this);
		req->_type = Request::Query;
		req->_id = id;
		req->_sdref = new ServiceRef;

		DNSServiceErrorType err = DNSServiceQueryRecord(
			req->_sdref->data(), kDNSServiceFlagsLongLivedQuery,
			0, name.constData(), qType, kDNSServiceClass_IN,
			cb_queryRecordReply, req);
		if(err != kDNSServiceErr_NoError)
		{
			setDelayedError(req, LowLevelError(
				"DNSServiceQueryRecord", err));
			return id;
		}

		req->_sdref->setInitialized();

		int sockfd = DNSServiceRefSockFD(*(req->_sdref->data()));
		if(sockfd == -1)
		{
			setDelayedError(req, LowLevelError(
				"DNSServiceRefSockFD", -1));
			return id;
		}

		req->_sockfd = sockfd;
		req->_sn_read = new SafeSocketNotifier(sockfd, QSocketNotifier::Read, this);
		connect(req->_sn_read, SIGNAL(activated(int)), SLOT(sn_activated()));
		_requestsById.insert(id, req);
		_requestsBySocket.insert(req->_sn_read, req);

		return id;
	}

	int browse(const QByteArray &serviceType, const QByteArray &domain)
	{
		int id = idManager.reserveId();

		Request *req = new Request(this);
		req->_type = Request::Browse;
		req->_id = id;
		req->_sdref = new ServiceRef;

		DNSServiceErrorType err = DNSServiceBrowse(
			req->_sdref->data(), 0, 0, serviceType.constData(),
			!domain.isEmpty() ? domain.constData() : NULL,
			cb_browseReply, req);
		if(err != kDNSServiceErr_NoError)
		{
			setDelayedError(req, LowLevelError(
				"DNSServiceBrowse", err));
			return id;
		}

		req->_sdref->setInitialized();

		int sockfd = DNSServiceRefSockFD(*(req->_sdref->data()));
		if(sockfd == -1)
		{
			setDelayedError(req, LowLevelError(
				"DNSServiceRefSockFD", -1));
			return id;
		}

		req->_sockfd = sockfd;
		req->_sn_read = new SafeSocketNotifier(sockfd, QSocketNotifier::Read, this);
		connect(req->_sn_read, SIGNAL(activated(int)), SLOT(sn_activated()));
		_requestsById.insert(id, req);
		_requestsBySocket.insert(req->_sn_read, req);

		return id;
	}

	int resolve(const QByteArray &serviceName, const QByteArray &serviceType, const QByteArray &domain)
	{
		int id = idManager.reserveId();

		Request *req = new Request(this);
		req->_type = Request::Resolve;
		req->_id = id;
		req->_sdref = new ServiceRef;

		DNSServiceErrorType err = DNSServiceResolve(
			req->_sdref->data(), 0, 0, serviceName.constData(),
			serviceType.constData(), domain.constData(),
			(DNSServiceResolveReply)cb_resolveReply, req);
		if(err != kDNSServiceErr_NoError)
		{
			setDelayedError(req, LowLevelError(
				"DNSServiceResolve", err));
			return id;
		}

		req->_sdref->setInitialized();

		int sockfd = DNSServiceRefSockFD(*(req->_sdref->data()));
		if(sockfd == -1)
		{
			setDelayedError(req, LowLevelError(
				"DNSServiceRefSockFD", -1));
			return id;
		}

		req->_sockfd = sockfd;
		req->_sn_read = new SafeSocketNotifier(sockfd, QSocketNotifier::Read, this);
		connect(req->_sn_read, SIGNAL(activated(int)), SLOT(sn_activated()));
		_requestsById.insert(id, req);
		_requestsBySocket.insert(req->_sn_read, req);

		return id;
	}

	int reg(const QByteArray &serviceName, const QByteArray &serviceType, const QByteArray &domain, int port, const QByteArray &txtRecord)
	{
		int id = idManager.reserveId();

		Request *req = new Request(this);
		req->_type = Request::Reg;
		req->_id = id;

		if(port < 1 || port > 0xffff)
		{
			setDelayedError(req, LowLevelError());
			return id;
		}

		uint16_t sport = port;
		sport = htons(sport);

		req->_sdref = new ServiceRef;

		DNSServiceErrorType err = DNSServiceRegister(
			req->_sdref->data(), kDNSServiceFlagsNoAutoRename, 0,
			serviceName.constData(), serviceType.constData(),
			domain.constData(), NULL, sport, txtRecord.size(),
			txtRecord.data(), cb_regReply, req);
		if(err != kDNSServiceErr_NoError)
		{
			setDelayedError(req, LowLevelError(
				"DNSServiceRegister", err));
			return id;
		}

		req->_sdref->setInitialized();

		int sockfd = DNSServiceRefSockFD(*(req->_sdref->data()));
		if(sockfd == -1)
		{
			setDelayedError(req, LowLevelError(
				"DNSServiceRefSockFD", -1));
			return id;
		}

		req->_sockfd = sockfd;
		req->_sn_read = new SafeSocketNotifier(sockfd, QSocketNotifier::Read, this);
		connect(req->_sn_read, SIGNAL(activated(int)), SLOT(sn_activated()));
		_requestsById.insert(id, req);
		_requestsBySocket.insert(req->_sn_read, req);

		return id;
	}

	int recordAdd(int reg_id, const Record &rec, LowLevelError *lowLevelError)
	{
		Request *req = _requestsById.value(reg_id);
		if(!req)
		{
			if(lowLevelError)
				*lowLevelError = LowLevelError();
			return -1;
		}

		RecordRef *recordRef = new RecordRef;

		DNSServiceErrorType err = DNSServiceAddRecord(
			*(req->_sdref->data()), recordRef->data(), 0,
			rec.rrtype, rec.rdata.size(), rec.rdata.data(),
			rec.ttl);
		if(err != kDNSServiceErr_NoError)
		{
			if(lowLevelError)
				*lowLevelError = LowLevelError("DNSServiceAddRecord", err);
			delete recordRef;
			return -1;
		}

		int id = idManager.reserveId();
		SubRecord *srec = new SubRecord(this);
		srec->_id = id;
		srec->_sdref = recordRef;
		req->_subRecords += srec;
		_requestsByRecId.insert(id, req);

		return id;
	}

	bool recordUpdate(int reg_id, int rec_id, const Record &rec, LowLevelError *lowLevelError)
	{
		Request *req = _requestsById.value(reg_id);
		if(!req)
		{
			if(lowLevelError)
				*lowLevelError = LowLevelError();
			return false;
		}

		SubRecord *srec = 0;
		if(rec_id != -1)
		{
			int at = req->subRecordIndexById(rec_id);
			if(at == -1)
			{
				if(lowLevelError)
					*lowLevelError = LowLevelError();
				return false;
			}
			srec = req->_subRecords[at];
		}

		DNSServiceErrorType err = DNSServiceUpdateRecord(
			*(req->_sdref->data()),
			srec ? *(srec->_sdref->data()) : NULL, 0,
			rec.rdata.size(), rec.rdata.data(), rec.ttl);
		if(err != kDNSServiceErr_NoError)
		{
			if(lowLevelError)
				*lowLevelError = LowLevelError("DNSServiceUpdateRecord", err);
			return false;
		}

		return true;
	}

	void recordRemove(int rec_id)
	{
		Request *req = _requestsByRecId.value(rec_id);
		if(!req)
			return;

		// this can't fail
		int at = req->subRecordIndexById(rec_id);

		SubRecord *srec = req->_subRecords[at];
		DNSServiceRemoveRecord(*(req->_sdref->data()), *(srec->_sdref->data()), 0);
		_requestsByRecId.remove(srec->_id);
		req->_subRecords.removeAt(at);
		delete srec;
	}

	void stop(int id)
	{
		Request *req = _requestsById.value(id);
		if(req)
			removeRequest(req);
	}

private slots:
	void sn_activated()
	{
		SafeSocketNotifier *sn_read = (SafeSocketNotifier *)sender();
		Request *req = _requestsBySocket.value(sn_read);
		if(!req)
			return;

		int id = req->_id;

		DNSServiceErrorType err = DNSServiceProcessResult(*(req->_sdref->data()));

		// do error if the above function returns an error, or if we
		//   collected an error during a callback
		if(err != kDNSServiceErr_NoError || !req->_lowLevelError.func.isEmpty())
		{
			LowLevelError lowLevelError;
			if(err != kDNSServiceErr_NoError)
				lowLevelError = LowLevelError("DNSServiceProcessResult", err);
			else
				lowLevelError = req->_lowLevelError;

			// reg conflict indicated via callback
			bool regConflict = false;
			if(req->_type == Request::Reg && !req->_lowLevelError.func.isEmpty())
				regConflict = req->_regConflict;

			removeRequest(req);

			if(req->_type == Request::Query)
			{
				QDnsSd::QueryResult r;
				r.success = false;
				r.lowLevelError = lowLevelError;
				emit q->queryResult(id, r);
			}
			else if(req->_type == Request::Browse)
			{
				QDnsSd::BrowseResult r;
				r.success = false;
				r.lowLevelError = lowLevelError;
				emit q->browseResult(id, r);
			}
			else if(req->_type == Request::Resolve)
			{
				QDnsSd::ResolveResult r;
				r.success = false;
				r.lowLevelError = lowLevelError;
				emit q->resolveResult(id, r);
			}
			else // Reg
			{
				QDnsSd::RegResult r;
				r.success = false;
				if(regConflict)
					r.errorCode = QDnsSd::RegResult::ErrorConflict;
				else
					r.errorCode = QDnsSd::RegResult::ErrorGeneric;
				r.lowLevelError = lowLevelError;
				emit q->regResult(id, r);
			}

			return;
		}

		// handle success

		if(req->_type == Request::Query)
		{
			if(req->_doSignal)
			{
				QDnsSd::QueryResult r;
				r.success = true;
				r.records = req->_queryRecords;
				req->_queryRecords.clear();
				req->_doSignal = false;
				emit q->queryResult(id, r);
			}
		}
		else if(req->_type == Request::Browse)
		{
			if(req->_doSignal)
			{
				QDnsSd::BrowseResult r;
				r.success = true;
				r.entries = req->_browseEntries;
				req->_browseEntries.clear();
				req->_doSignal = false;
				emit q->browseResult(id, r);
			}
		}
		else if(req->_type == Request::Resolve)
		{
			if(req->_doSignal)
			{
				QDnsSd::ResolveResult r;
				r.success = true;
				r.fullName = req->_resolveFullName;
				r.hostTarget = req->_resolveHost;
				r.port = req->_resolvePort;
				r.txtRecord = req->_resolveTxtRecord;
				req->_doSignal = false;

				// there is only one response
				removeRequest(req);

				emit q->resolveResult(id, r);
			}
		}
		else // Reg
		{
			if(req->_doSignal)
			{
				QDnsSd::RegResult r;
				r.success = true;
				r.domain = req->_regDomain;
				req->_doSignal = false;

				emit q->regResult(id, r);
			}
		}
	}

	void doError()
	{
		SafeTimer *t = (SafeTimer *)sender();
		Request *req = _requestsByTimer.value(t);
		if(!req)
			return;

		int id = req->_id;
		int type = req->_type;
		removeRequest(req);

		if(type == Request::Query)
		{
			QDnsSd::QueryResult r;
			r.success = false;
			r.lowLevelError = req->_lowLevelError;
			emit q->queryResult(id, r);
		}
		else if(type == Request::Browse)
		{
			QDnsSd::BrowseResult r;
			r.success = false;
			r.lowLevelError = req->_lowLevelError;
			emit q->browseResult(id, r);
		}
		else if(type == Request::Resolve)
		{
			QDnsSd::ResolveResult r;
			r.success = false;
			r.lowLevelError = req->_lowLevelError;
			emit q->resolveResult(id, r);
		}
		else // Reg
		{
			QDnsSd::RegResult r;
			r.success = false;
			r.errorCode = QDnsSd::RegResult::ErrorGeneric;
			r.lowLevelError = req->_lowLevelError;
			emit q->regResult(id, r);
		}
	}

private:
	static void cb_queryRecordReply(DNSServiceRef ref,
		DNSServiceFlags flags, uint32_t interfaceIndex,
		DNSServiceErrorType errorCode, const char *fullname,
		uint16_t rrtype, uint16_t rrclass, uint16_t rdlen,
		const void *rdata, uint32_t ttl, void *context)
	{
		Q_UNUSED(ref);
		Q_UNUSED(interfaceIndex);
		Q_UNUSED(rrclass);

		Request *req = static_cast<Request *>(context);
		req->_self->handle_queryRecordReply(req, flags, errorCode,
			fullname, rrtype, rdlen, (const char *)rdata, ttl);
	}

	static void cb_browseReply(DNSServiceRef ref,
		DNSServiceFlags flags, uint32_t interfaceIndex,
		DNSServiceErrorType errorCode, const char *serviceName,
		const char *regtype, const char *replyDomain, void *context)
	{
		Q_UNUSED(ref);
		Q_UNUSED(interfaceIndex);

		Request *req = static_cast<Request *>(context);
		req->_self->handle_browseReply(req, flags, errorCode,
			serviceName, regtype, replyDomain);
	}

	static void cb_resolveReply(DNSServiceRef ref,
		DNSServiceFlags flags, uint32_t interfaceIndex,
		DNSServiceErrorType errorCode, const char *fullname,
		const char *hosttarget, uint16_t port, uint16_t txtLen,
		const unsigned char *txtRecord, void *context)
	{
		Q_UNUSED(ref);
		Q_UNUSED(flags);
		Q_UNUSED(interfaceIndex);

		Request *req = static_cast<Request *>(context);
		req->_self->handle_resolveReply(req, errorCode, fullname,
			hosttarget, port, txtLen, txtRecord);
	}

	static void cb_regReply(DNSServiceRef ref,
		DNSServiceFlags flags, DNSServiceErrorType errorCode,
		const char *name, const char *regtype, const char *domain,
		void *context)
	{
		Q_UNUSED(ref);
		Q_UNUSED(flags);

		Request *req = static_cast<Request *>(context);
		req->_self->handle_regReply(req, errorCode, name, regtype,
			domain);
	}

	void handle_queryRecordReply(Request *req, DNSServiceFlags flags,
		DNSServiceErrorType errorCode, const char *fullname,
		uint16_t rrtype, uint16_t rdlen, const char *rdata,
		uint16_t ttl)
	{
		if(errorCode != kDNSServiceErr_NoError)
		{
			req->_doSignal = true;
			req->_lowLevelError = LowLevelError("DNSServiceQueryRecordReply", errorCode);
			return;
		}

		QDnsSd::Record rec;
		rec.added = (flags & kDNSServiceFlagsAdd) ? true: false;
		rec.name = QByteArray(fullname);
		rec.rrtype = rrtype;
		rec.rdata = QByteArray(rdata, rdlen);
		rec.ttl = ttl;
		req->_queryRecords += rec;

		if(!(flags & kDNSServiceFlagsMoreComing))
			req->_doSignal = true;
	}

	void handle_browseReply(Request *req, DNSServiceFlags flags,
		DNSServiceErrorType errorCode, const char *serviceName,
		const char *regtype, const char *replyDomain)
	{
		if(errorCode != kDNSServiceErr_NoError)
		{
			req->_doSignal = true;
			req->_lowLevelError = LowLevelError("DNSServiceBrowseReply", errorCode);
			return;
		}

		QDnsSd::BrowseEntry e;
		e.added = (flags & kDNSServiceFlagsAdd) ? true: false;
		e.serviceName = QByteArray(serviceName);
		e.serviceType = QByteArray(regtype);
		e.replyDomain = QByteArray(replyDomain);
		req->_browseEntries += e;

		if(!(flags & kDNSServiceFlagsMoreComing))
			req->_doSignal = true;
	}

	void handle_resolveReply(Request *req, DNSServiceErrorType errorCode,
		const char *fullname, const char *hosttarget, uint16_t port,
		uint16_t txtLen, const unsigned char *txtRecord)
	{
		if(errorCode != kDNSServiceErr_NoError)
		{
			req->_doSignal = true;
			req->_lowLevelError = LowLevelError("DNSServiceResolveReply", errorCode);
			return;
		}

		req->_resolveFullName = QByteArray(fullname);
		req->_resolveHost = QByteArray(hosttarget);
		req->_resolvePort = ntohs(port);
		req->_resolveTxtRecord = QByteArray((const char *)txtRecord, txtLen);

		req->_doSignal = true;
	}

	void handle_regReply(Request *req, DNSServiceErrorType errorCode,
		const char *name, const char *regtype, const char *domain)
	{
		Q_UNUSED(name);
		Q_UNUSED(regtype);

		if(errorCode != kDNSServiceErr_NoError)
		{
			req->_doSignal = true;
			req->_lowLevelError = LowLevelError("DNSServiceRegisterReply", errorCode);

			if(errorCode == kDNSServiceErr_NameConflict)
				req->_regConflict = true;
			else
				req->_regConflict = false;
			return;
		}

		req->_regDomain = QByteArray(domain);
		req->_doSignal = true;
	}
};

QDnsSd::QDnsSd(QObject *parent) :
	QObject(parent)
{
	d = new Private(this);
}

QDnsSd::~QDnsSd()
{
	delete d;
}

int QDnsSd::query(const QByteArray &name, int qType)
{
	return d->query(name, qType);
}

int QDnsSd::browse(const QByteArray &serviceType, const QByteArray &domain)
{
	return d->browse(serviceType, domain);
}

int QDnsSd::resolve(const QByteArray &serviceName, const QByteArray &serviceType, const QByteArray &domain)
{
	return d->resolve(serviceName, serviceType, domain);
}

int QDnsSd::reg(const QByteArray &serviceName, const QByteArray &serviceType, const QByteArray &domain, int port, const QByteArray &txtRecord)
{
	return d->reg(serviceName, serviceType, domain, port, txtRecord);
}

int QDnsSd::recordAdd(int reg_id, const Record &rec, LowLevelError *lowLevelError)
{
	return d->recordAdd(reg_id, rec, lowLevelError);
}

bool QDnsSd::recordUpdate(int rec_id, const Record &rec, LowLevelError *lowLevelError)
{
	int reg_id = d->regIdForRecId(rec_id);
	if(reg_id == -1)
		return false;

	return d->recordUpdate(reg_id, rec_id, rec, lowLevelError);
}

bool QDnsSd::recordUpdateTxt(int reg_id, const QByteArray &txtRecord, quint32 ttl, LowLevelError *lowLevelError)
{
	Record rec;
	rec.rrtype = kDNSServiceType_TXT;
	rec.rdata = txtRecord;
	rec.ttl = ttl;
	return d->recordUpdate(reg_id, -1, rec, lowLevelError);
}

void QDnsSd::recordRemove(int rec_id)
{
	d->recordRemove(rec_id);
}

void QDnsSd::stop(int id)
{
	d->stop(id);
}

QByteArray QDnsSd::createTxtRecord(const QList<QByteArray> &strings)
{
	// split into var/val and validate
	QList<QByteArray> vars;
	QList<QByteArray> vals; // null = no value, empty = empty value
	foreach(const QByteArray &i, strings)
	{
		QByteArray var;
		QByteArray val;
		int n = i.indexOf('=');
		if(n != -1)
		{
			var = i.mid(0, n);
			val = i.mid(n + 1);
		}
		else
			var = i;

		for(int n = 0; n < var.size(); ++n)
		{
			unsigned char c = var[n];
			if(c < 0x20 || c > 0x7e)
				return QByteArray();
		}

		vars += var;
		vals += val;
	}

	TXTRecordRef ref;
	QByteArray buf(256, 0);
	TXTRecordCreate(&ref, buf.size(), buf.data());
	for(int n = 0; n < vars.count(); ++n)
	{
		int valueSize = vals[n].size();
		char *value;
		if(!vals[n].isNull())
			value = vals[n].data();
		else
			value = 0;

		DNSServiceErrorType err = TXTRecordSetValue(&ref,
			vars[n].data(), valueSize, value);
		if(err != kDNSServiceErr_NoError)
		{
			TXTRecordDeallocate(&ref);
			return QByteArray();
		}
	}
	QByteArray out((const char *)TXTRecordGetBytesPtr(&ref), TXTRecordGetLength(&ref));
	TXTRecordDeallocate(&ref);
	return out;
}

QList<QByteArray> QDnsSd::parseTxtRecord(const QByteArray &txtRecord)
{
	QList<QByteArray> out;
	int count = TXTRecordGetCount(txtRecord.size(), txtRecord.data());
	for(int n = 0; n < count; ++n)
	{
		QByteArray keyBuf(256, 0);
		uint8_t valueLen;
		const void *value;
		DNSServiceErrorType err = TXTRecordGetItemAtIndex(
			txtRecord.size(), txtRecord.data(), n, keyBuf.size(),
			keyBuf.data(), &valueLen, &value);
		if(err != kDNSServiceErr_NoError)
			return QList<QByteArray>();

		keyBuf.resize(qstrlen(keyBuf.data()));

		QByteArray entry = keyBuf;
		if(value)
		{
			entry += '=';
			entry += QByteArray((const char *)value, valueLen);
		}
		out += entry;
	}
	return out;
}

#include "qdnssd.moc"
