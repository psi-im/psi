/*
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
 * Copyright (C) 2004-2006  Brad Hards <bradh@frogmouth.net>
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

#include "qca_cert.h"

#include <QtCore>
#include "qca_publickey.h"
#include "qcaprovider.h"

namespace QCA {

Provider::Context *getContext(const QString &type, const QString &provider);

// from qca_publickey.cpp
bool stringToFile(const QString &fileName, const QString &content);
bool stringFromFile(const QString &fileName, QString *s);
bool arrayToFile(const QString &fileName, const QByteArray &content);
bool arrayFromFile(const QString &fileName, QByteArray *a);
bool ask_passphrase(const QString &fname, void *ptr, QSecureArray *answer);

//----------------------------------------------------------------------------
// CertificateOptions
//----------------------------------------------------------------------------
class CertificateOptions::Private
{
public:
	CertificateRequestFormat format;

	QString challenge;
	CertificateInfo info;
	Constraints constraints;
	QStringList policies;
	bool isCA;
	int pathLimit;
	QBigInteger serial;
	QDateTime start, end;

	Private() : isCA(false), pathLimit(0)
	{
	}
};

CertificateOptions::CertificateOptions(CertificateRequestFormat f)
{
	d = new Private;
	d->format = f;
}

CertificateOptions::CertificateOptions(const CertificateOptions &from)
{
	d = new Private(*from.d);
}

CertificateOptions::~CertificateOptions()
{
	delete d;
}

CertificateOptions & CertificateOptions::operator=(const CertificateOptions &from)
{
	*d = *from.d;
	return *this;
}

CertificateRequestFormat CertificateOptions::format() const
{
	return d->format;
}

void CertificateOptions::setFormat(CertificateRequestFormat f)
{
	d->format = f;
}

bool CertificateOptions::isValid() const
{
	// logic from Botan
	if(d->info.value(CommonName).isEmpty() || d->info.value(Country).isEmpty())
		return false;
	if(d->info.value(Country).length() != 2)
		return false;
	if(d->start >= d->end)
		return false;
	return true;
}

QString CertificateOptions::challenge() const
{
	return d->challenge;
}

CertificateInfo CertificateOptions::info() const
{
	return d->info;
}

Constraints CertificateOptions::constraints() const
{
	return d->constraints;
}

QStringList CertificateOptions::policies() const
{
	return d->policies;
}

bool CertificateOptions::isCA() const
{
	return d->isCA;
}

int CertificateOptions::pathLimit() const
{
	return d->pathLimit;
}

QBigInteger CertificateOptions::serialNumber() const
{
	return d->serial;
}

QDateTime CertificateOptions::notValidBefore() const
{
	return d->start;
}

QDateTime CertificateOptions::notValidAfter() const
{
	return d->end;
}

void CertificateOptions::setChallenge(const QString &s)
{
	d->challenge = s;
}

void CertificateOptions::setInfo(const CertificateInfo &info)
{
	d->info = info;
}

void CertificateOptions::setConstraints(const Constraints &constraints)
{
	d->constraints = constraints;
}

void CertificateOptions::setPolicies(const QStringList &policies)
{
	d->policies = policies;
}

void CertificateOptions::setAsCA(int pathLimit)
{
	d->isCA = true;
	d->pathLimit = pathLimit;
}

void CertificateOptions::setAsUser()
{
	d->isCA = false;
	d->pathLimit = 0;
}

void CertificateOptions::setSerialNumber(const QBigInteger &i)
{
	d->serial = i;
}

void CertificateOptions::setValidityPeriod(const QDateTime &start, const QDateTime &end)
{
	d->start = start;
	d->end = end;
}

//----------------------------------------------------------------------------
// Certificate
//----------------------------------------------------------------------------
// (adapted from kdelibs) -- Justin
static bool certnameMatchesAddress(const QString &_cn, const QString &peerHost)
{
	QString cn = _cn.trimmed().toLower();
	QRegExp rx;

	// Check for invalid characters
	if(QRegExp("[^a-zA-Z0-9\\.\\*\\-]").indexIn(cn) >= 0)
		return false;

	// Domains can legally end with '.'s.  We don't need them though.
	while(cn.endsWith("."))
		cn.truncate(cn.length()-1);

	// Do not let empty CN's get by!!
	if(cn.isEmpty())
		return false;

	// Check for IPv4 address
	rx.setPattern("[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}");
	if(rx.exactMatch(peerHost))
		return peerHost == cn;

	// Check for IPv6 address here...
	rx.setPattern("^\\[.*\\]$");
	if(rx.exactMatch(peerHost))
		return peerHost == cn;

	if(cn.contains('*'))
	{
		// First make sure that there are at least two valid parts
		// after the wildcard (*).
		QStringList parts = cn.split('.', QString::SkipEmptyParts);

		while(parts.count() > 2)
			parts.removeFirst();

		if(parts.count() != 2)
			return false;  // we don't allow *.root - that's bad

		if(parts[0].contains('*') || parts[1].contains('*'))
			return false;

		// RFC2818 says that *.example.com should match against
		// foo.example.com but not bar.foo.example.com
		// (ie. they must have the same number of parts)
		if(QRegExp(cn, Qt::CaseInsensitive, QRegExp::Wildcard).exactMatch(peerHost) &&
		   cn.split('.', QString::SkipEmptyParts).count() ==
		   peerHost.split('.', QString::SkipEmptyParts).count())
			return true;

		return false;
	}

	// We must have an exact match in this case (insensitive though)
	// (note we already did .lower())
	if(cn == peerHost)
		return true;
	return false;
}

Certificate::Certificate()
{
}

Certificate::Certificate(const QString &fileName)
{
	*this = fromPEMFile(fileName, 0, QString());
}

Certificate::Certificate(const CertificateOptions &opts, const PrivateKey &key, const QString &provider)
{
	CertContext *c = static_cast<CertContext *>(getContext("cert", provider));
	if(c->createSelfSigned(opts, *(static_cast<const PKeyContext *>(key.context()))))
		change(c);
	else
		delete c;
}

bool Certificate::isNull() const
{
	return (!context() ? true : false);
}

QDateTime Certificate::notValidBefore() const
{
	return static_cast<const CertContext *>(context())->props()->start;
}

QDateTime Certificate::notValidAfter() const
{
	return static_cast<const CertContext *>(context())->props()->end;
}

CertificateInfo Certificate::subjectInfo() const
{
	return static_cast<const CertContext *>(context())->props()->subject;
}

CertificateInfo Certificate::issuerInfo() const
{
	return static_cast<const CertContext *>(context())->props()->issuer;
}

Constraints Certificate::constraints() const
{
	return static_cast<const CertContext *>(context())->props()->constraints;
}

QStringList Certificate::policies() const
{
	return static_cast<const CertContext *>(context())->props()->policies;
}

QString Certificate::commonName() const
{
	return static_cast<const CertContext *>(context())->props()->subject.value(CommonName);
}

QBigInteger Certificate::serialNumber() const
{
	return static_cast<const CertContext *>(context())->props()->serial;
}

PublicKey Certificate::subjectPublicKey() const
{
	PKeyContext *c = static_cast<const CertContext *>(context())->subjectPublicKey();
	PublicKey key;
	key.change(c);
	return key;
}

bool Certificate::isCA() const
{
	return static_cast<const CertContext *>(context())->props()->isCA;
}

bool Certificate::isSelfSigned() const
{
	return static_cast<const CertContext *>(context())->props()->isSelfSigned;
}

bool Certificate::isIssuerOf(const Certificate &other) const
{
	const CertContext *cc = static_cast<const CertContext *>(other.context());
	return static_cast<const CertContext *>(context())->isIssuerOf(cc);
}

int Certificate::pathLimit() const
{
	return static_cast<const CertContext *>(context())->props()->pathLimit;
}

SignatureAlgorithm Certificate::signatureAlgorithm() const
{
	return static_cast<const CertContext *>(context())->props()->sigalgo;
}

QByteArray Certificate::subjectKeyId() const
{
	return static_cast<const CertContext *>(context())->props()->subjectId;
}

QByteArray Certificate::issuerKeyId() const
{
	return static_cast<const CertContext *>(context())->props()->issuerId;
}

Validity Certificate::validate(const CertificateCollection &trusted, const CertificateCollection &untrusted, UsageMode u) const
{
	QList<Certificate> issuers = trusted.certificates() + untrusted.certificates();
	CertificateChain chain;
	chain += *this;
	chain = chain.complete(issuers);
	return chain.validate(trusted, untrusted.crls(), u);

	/*QList<CertContext*> trusted_list;
	QList<CertContext*> untrusted_list;
	QList<CRLContext*> crl_list;

	QList<Certificate> trusted_certs = trusted.certificates();
	QList<Certificate> untrusted_certs = untrusted.certificates();
	QList<CRL> crls = trusted.crls() + untrusted.crls();

	int n;
	for(n = 0; n < trusted_certs.count(); ++n)
	{
		CertContext *c = static_cast<CertContext *>(trusted_certs[n].context());
		trusted_list += c;
	}
	for(n = 0; n < untrusted_certs.count(); ++n)
	{
		CertContext *c = static_cast<CertContext *>(untrusted_certs[n].context());
		untrusted_list += c;
	}
	for(n = 0; n < crls.count(); ++n)
	{
		CRLContext *c = static_cast<CRLContext *>(crls[n].context());
		crl_list += c;
	}

	return static_cast<const CertContext *>(context())->validate(trusted_list, untrusted_list, crl_list, u);*/
}

QSecureArray Certificate::toDER() const
{
	return static_cast<const CertContext *>(context())->toDER();
}

QString Certificate::toPEM() const
{
	return static_cast<const CertContext *>(context())->toPEM();
}

bool Certificate::toPEMFile(const QString &fileName) const
{
	return stringToFile(fileName, toPEM());
}

Certificate Certificate::fromDER(const QSecureArray &a, ConvertResult *result, const QString &provider)
{
	Certificate c;
	CertContext *cc = static_cast<CertContext *>(getContext("cert", provider));
	ConvertResult r = cc->fromDER(a);
	if(result)
		*result = r;
	if(r == ConvertGood)
		c.change(cc);
	return c;
}

Certificate Certificate::fromPEM(const QString &s, ConvertResult *result, const QString &provider)
{
	Certificate c;
	CertContext *cc = static_cast<CertContext *>(getContext("cert", provider));
	ConvertResult r = cc->fromPEM(s);
	if(result)
		*result = r;
	if(r == ConvertGood)
		c.change(cc);
	return c;
}

Certificate Certificate::fromPEMFile(const QString &fileName, ConvertResult *result, const QString &provider)
{
	QString pem;
	if(!stringFromFile(fileName, &pem))
	{
		if(result)
			*result = ErrorFile;
		return Certificate();
	}
	return fromPEM(pem, result, provider);
}

bool Certificate::matchesHostname(const QString &realHost) const
{
	QString peerHost = realHost.trimmed();
	while(peerHost.endsWith("."))
		peerHost.truncate(peerHost.length()-1);
	peerHost = peerHost.toLower();

	if(certnameMatchesAddress(commonName(), peerHost))
		return true;

	foreach( const QString &dnsName, subjectInfo().values(DNS) ) {
		if (certnameMatchesAddress(dnsName, peerHost))
			return true;
	}

	foreach( const QString &ipAddy, subjectInfo().values(IPAddress) ) {
		if (certnameMatchesAddress(ipAddy, peerHost))
			return true;
	}


	return false;
}

bool Certificate::operator==(const Certificate &otherCert) const
{
	if ( isNull() ) {
		if ( otherCert.isNull() ) {
			return true;
		} else {
			return false;
		}
	} else if ( otherCert.isNull() ) {
		return false;
	}

	const CertContextProps *a = static_cast<const CertContext *>(context())->props();
	const CertContextProps *b = static_cast<const CertContext *>(otherCert.context())->props();

	// logic from Botan
	if(a->sig != b->sig || a->sigalgo != b->sigalgo || 
	   subjectPublicKey() != otherCert.subjectPublicKey())
		return false;
	if(a->issuer != b->issuer || a->subject != b->subject)
		return false;
	if(a->serial != b->serial || a->version != b->version)
		return false;
	if(a->start != b->start || a->end != b->end)
		return false;
	return true;
}

bool Certificate::operator!=(const Certificate &a) const
{
	return !(*this == a);
}

Validity Certificate::chain_validate(const CertificateChain &chain, const CertificateCollection &trusted, const QList<CRL> &untrusted_crls, UsageMode u) const
{
	QList<CertContext*> chain_list;
	QList<CertContext*> trusted_list;
	QList<CRLContext*> crl_list;

	QList<Certificate> chain_certs = chain;
	QList<Certificate> trusted_certs = trusted.certificates();
	QList<CRL> crls = trusted.crls() + untrusted_crls;

	for(int n = 0; n < chain_certs.count(); ++n)
	{
		CertContext *c = static_cast<CertContext *>(chain_certs[n].context());
		chain_list += c;
	}
	for(int n = 0; n < trusted_certs.count(); ++n)
	{
		CertContext *c = static_cast<CertContext *>(trusted_certs[n].context());
		trusted_list += c;
	}
	for(int n = 0; n < crls.count(); ++n)
	{
		CRLContext *c = static_cast<CRLContext *>(crls[n].context());
		crl_list += c;
	}

	return static_cast<const CertContext *>(context())->validate_chain(chain_list, trusted_list, crl_list, u);
}

CertificateChain Certificate::chain_complete(const CertificateChain &chain, const QList<Certificate> &issuers) const
{
	CertificateChain out;
	QList<Certificate> pool = issuers + chain.mid(1);
	out += chain.first();
	while(!out.last().isSelfSigned())
	{
		// try to get next in chain
		int at = -1;
		for(int n = 0; n < pool.count(); ++n)
		{
			//QString str = QString("[%1] issued by [%2] ? ").arg(out.last().commonName()).arg(pool[n].commonName());
			if(pool[n].isIssuerOf(out.last()))
			{
				//printf("%s  yes\n", qPrintable(str));
				at = n;
				break;
			}
			//printf("%s  no\n", qPrintable(str));
		}
		if(at == -1)
			break;

		// take it out of the pool
		Certificate next = pool.takeAt(at);

		// make sure it isn't in the chain already (avoid loops)
		if(out.contains(next))
			break;

		// append to the chain
		out += next;
	}
	return out;
}

//----------------------------------------------------------------------------
// CertificateRequest
//----------------------------------------------------------------------------
CertificateRequest::CertificateRequest()
{
}

CertificateRequest::CertificateRequest(const QString &fileName)
{
	*this = fromPEMFile(fileName, 0, QString());
}

CertificateRequest::CertificateRequest(const CertificateOptions &opts, const PrivateKey &key, const QString &provider)
{
	CSRContext *c = static_cast<CSRContext *>(getContext("csr", provider));
	if(c->createRequest(opts, *(static_cast<const PKeyContext *>(key.context()))))
		change(c);
	else
		delete c;
}

bool CertificateRequest::isNull() const
{
	return (!context() ? true : false);
}

bool CertificateRequest::canUseFormat(CertificateRequestFormat f, const QString &provider)
{
	CSRContext *c = static_cast<CSRContext *>(getContext("csr", provider));
	bool ok = c->canUseFormat(f);
	delete c;
	return ok;
}

CertificateRequestFormat CertificateRequest::format() const
{
	if(isNull())
		return PKCS10; // some default so we don't explode
	return static_cast<const CSRContext *>(context())->props()->format;
}

CertificateInfo CertificateRequest::subjectInfo() const
{
	return static_cast<const CSRContext *>(context())->props()->subject;
}

Constraints CertificateRequest::constraints() const
{
	return static_cast<const CSRContext *>(context())->props()->constraints;
}

QStringList CertificateRequest::policies() const
{
	return static_cast<const CSRContext *>(context())->props()->policies;
}

PublicKey CertificateRequest::subjectPublicKey() const
{
	PKeyContext *c = static_cast<const CSRContext *>(context())->subjectPublicKey();
	PublicKey key;
	key.change(c);
	return key;
}

bool CertificateRequest::isCA() const
{
	return static_cast<const CSRContext *>(context())->props()->isCA;
}

int CertificateRequest::pathLimit() const
{
	return static_cast<const CSRContext *>(context())->props()->pathLimit;
}

QString CertificateRequest::challenge() const
{
	return static_cast<const CSRContext *>(context())->props()->challenge;
}

SignatureAlgorithm CertificateRequest::signatureAlgorithm() const
{
	return static_cast<const CSRContext *>(context())->props()->sigalgo;
}

bool CertificateRequest::operator==(const CertificateRequest &otherCsr) const
{
	if (isNull()) {
		if (otherCsr.isNull())
			// they are both null
			return true;
		else
			return false;
	}
	if (otherCsr.isNull())
		return false;

	if (signatureAlgorithm() != otherCsr.signatureAlgorithm())
		return false;

	const CertContextProps *a = static_cast<const CSRContext *>(context())->props();
	const CertContextProps *b = static_cast<const CSRContext *>(otherCsr.context())->props();

	if (a->sig != b->sig)
		return false;

	// TODO: Anything else we should compare?

	return true;
}

QSecureArray CertificateRequest::toDER() const
{
	return static_cast<const CSRContext *>(context())->toDER();
}

QString CertificateRequest::toPEM() const
{
	return static_cast<const CSRContext *>(context())->toPEM();
}

bool CertificateRequest::toPEMFile(const QString &fileName) const
{
	return stringToFile(fileName, toPEM());
}

CertificateRequest CertificateRequest::fromDER(const QSecureArray &a, ConvertResult *result, const QString &provider)
{
	CertificateRequest c;
	CSRContext *csr = static_cast<CSRContext *>(getContext("csr", provider));
	ConvertResult r = csr->fromDER(a);
	if(result)
		*result = r;
	if(r == ConvertGood)
		c.change(csr);
	return c;
}

CertificateRequest CertificateRequest::fromPEM(const QString &s, ConvertResult *result, const QString &provider)
{
	CertificateRequest c;
	CSRContext *csr = static_cast<CSRContext *>(getContext("csr", provider));
	ConvertResult r = csr->fromPEM(s);
	if(result)
		*result = r;
	if(r == ConvertGood)
		c.change(csr);
	return c;
}

CertificateRequest CertificateRequest::fromPEMFile(const QString &fileName, ConvertResult *result, const QString &provider)
{
	QString pem;
	if(!stringFromFile(fileName, &pem))
	{
		if(result)
			*result = ErrorFile;
		return CertificateRequest();
	}
	return fromPEM(pem, result, provider);
}

QString CertificateRequest::toString() const
{
	return static_cast<const CSRContext *>(context())->toSPKAC();
}

CertificateRequest CertificateRequest::fromString(const QString &s, ConvertResult *result, const QString &provider)
{
	CertificateRequest c;
	CSRContext *csr = static_cast<CSRContext *>(getContext("csr", provider));
	ConvertResult r = csr->fromSPKAC(s);
	if(result)
		*result = r;
	if(r == ConvertGood)
		c.change(csr);
	return c;
}

//----------------------------------------------------------------------------
// CRLEntry
//----------------------------------------------------------------------------
CRLEntry::CRLEntry()
{
	_reason = Unspecified;
}

bool CRLEntry::isNull() const
{
	return (_time.isNull());
}

CRLEntry::CRLEntry(const Certificate &c, Reason r)
{
	_serial = c.serialNumber();
	_time = QDateTime::currentDateTime();
	_reason = r;
}

CRLEntry::CRLEntry(const QBigInteger serial, const QDateTime time, Reason r)
{
	_serial = serial;
	_time = time;
	_reason = r;
}

QBigInteger CRLEntry::serialNumber() const
{
	return _serial;
}

QDateTime CRLEntry::time() const
{
	return _time;
}

CRLEntry::Reason CRLEntry::reason() const
{
	return _reason;
}

bool CRLEntry::operator==(const CRLEntry &otherEntry) const
{
	if ( isNull() ) {
		if ( otherEntry.isNull() ) {
			return true;
		} else {
			return false;
		}
	} else if ( otherEntry.isNull() ) {
		return false;
	}

	if ( ( _serial != otherEntry.serialNumber() ) ||
	     ( _time != otherEntry.time() ) ||
	     ( _reason != otherEntry.reason() ) ) {
		return false;
	}
	return true;

}

bool CRLEntry::operator<(const CRLEntry &otherEntry) const
{
	if ( isNull() || otherEntry.isNull() ) {
		return false;
	}

	if ( _serial < otherEntry.serialNumber() ) {
		return true;
	}

	return false;
}

//----------------------------------------------------------------------------
// CRL
//----------------------------------------------------------------------------
CRL::CRL()
{
}

bool CRL::isNull() const
{
	return (!context() ? true : false);
}

CertificateInfo CRL::issuerInfo() const
{
	return static_cast<const CRLContext *>(context())->props()->issuer;
}

int CRL::number() const
{
	return static_cast<const CRLContext *>(context())->props()->number;
}

QDateTime CRL::thisUpdate() const
{
	return static_cast<const CRLContext *>(context())->props()->thisUpdate;
}

QDateTime CRL::nextUpdate() const
{
	return static_cast<const CRLContext *>(context())->props()->nextUpdate;
}

QList<CRLEntry> CRL::revoked() const
{
	return static_cast<const CRLContext *>(context())->props()->revoked;
}

QSecureArray CRL::signature() const
{
	return static_cast<const CRLContext *>(context())->props()->sig;
}

SignatureAlgorithm CRL::signatureAlgorithm() const
{
	return static_cast<const CRLContext *>(context())->props()->sigalgo;
}

QByteArray CRL::issuerKeyId() const
{
	return static_cast<const CRLContext *>(context())->props()->issuerId;
}

QSecureArray CRL::toDER() const
{
	return static_cast<const CRLContext *>(context())->toDER();
}

QString CRL::toPEM() const
{
	return static_cast<const CRLContext *>(context())->toPEM();
}

bool CRL::operator==(const CRL &otherCrl) const
{
	if ( isNull() ) {
		if ( otherCrl.isNull() ) {
			return true;
		} else {
			return false;
		}
	} else if ( otherCrl.isNull() ) {
		return false;
	}

	if ( number() != otherCrl.number() )
		return false;

	if ( thisUpdate() != otherCrl.thisUpdate() )
		return false;

	if ( nextUpdate() != otherCrl.nextUpdate() )
		return false;

	if ( signature() != otherCrl.signature() )
		return false;

	if ( signatureAlgorithm() != otherCrl.signatureAlgorithm() )
		return false;

	if ( issuerKeyId() != otherCrl.issuerKeyId() )
		return false;

	if ( revoked() != otherCrl.revoked() )
		return false;

	if ( issuerKeyId() != otherCrl.issuerKeyId() )
		return false;

	return true;

}


CRL CRL::fromDER(const QSecureArray &a, ConvertResult *result, const QString &provider)
{
	CRL c;
	CRLContext *cc = static_cast<CRLContext *>(getContext("crl", provider));
	ConvertResult r = cc->fromDER(a);
	if(result)
		*result = r;
	if(r == ConvertGood)
		c.change(cc);
	return c;
}

CRL CRL::fromPEM(const QString &s, ConvertResult *result, const QString &provider)
{
	CRL c;
	CRLContext *cc = static_cast<CRLContext *>(getContext("crl", provider));
	ConvertResult r = cc->fromPEM(s);
	if(result)
		*result = r;
	if(r == ConvertGood)
		c.change(cc);
	return c;
}

CRL CRL::fromPEMFile(const QString &fileName, ConvertResult *result, const QString &provider)
{
	QString pem;
	if(!stringFromFile(fileName, &pem))
	{
		if(result)
			*result = ErrorFile;
		return CRL();
	}
	return fromPEM(pem, result, provider);
}

bool CRL::toPEMFile(const QString &fileName) const
{
	return stringToFile(fileName, toPEM());
}


//----------------------------------------------------------------------------
// Store
//----------------------------------------------------------------------------
// TODO: support CRLs
// CRL / X509 CRL
// CERTIFICATE / X509 CERTIFICATE
static QString readNextPem(QTextStream *ts, bool *isCRL)
{
	QString pem;
	bool found = false;
	bool done = false;
	*isCRL = false;
	while(!ts->atEnd())
	{
		QString line = ts->readLine();
		if(!found)
		{
			if(line == "-----BEGIN CERTIFICATE-----")
			{
				found = true;
				pem += line + '\n';
			}
		}
		else
		{
			pem += line + '\n';
			if(line == "-----END CERTIFICATE-----")
			{
				done = true;
				break;
			}
		}
	}
	if(!done)
		return QString::null;
	return pem;
}

class CertificateCollection::Private : public QSharedData
{
public:
	QList<Certificate> certs;
	QList<CRL> crls;
};

CertificateCollection::CertificateCollection()
:d(new Private)
{
}

CertificateCollection::CertificateCollection(const CertificateCollection &from)
:d(from.d)
{
}

CertificateCollection::~CertificateCollection()
{
}

CertificateCollection & CertificateCollection::operator=(const CertificateCollection &from)
{
	d = from.d;
	return *this;
}

void CertificateCollection::addCertificate(const Certificate &cert)
{
	d->certs.append(cert);
}

void CertificateCollection::addCRL(const CRL &crl)
{
	d->crls.append(crl);
}

QList<Certificate> CertificateCollection::certificates() const
{
	return d->certs;
}

QList<CRL> CertificateCollection::crls() const
{
	return d->crls;
}

void CertificateCollection::append(const CertificateCollection &other)
{
	d->certs += other.d->certs;
	d->crls += other.d->crls;
}

CertificateCollection CertificateCollection::operator+(const CertificateCollection &other) const
{
	CertificateCollection c = *this;
	c.append(other);
	return c;
}

CertificateCollection & CertificateCollection::operator+=(const CertificateCollection &other)
{
	append(other);
	return *this;
}

bool CertificateCollection::canUsePKCS7(const QString &provider)
{
	CertCollectionContext *c = static_cast<CertCollectionContext *>(getContext("certcollection", provider));
	bool ok = c ? true : false;
	delete c;
	return ok;
}

bool CertificateCollection::toFlatTextFile(const QString &fileName)
{
	QFile f(fileName);
	if(!f.open(QFile::WriteOnly))
		return false;

	QTextStream ts(&f);
	int n;
	for(n = 0; n < d->certs.count(); ++n)
		ts << d->certs[n].toPEM();
	for(n = 0; n < d->crls.count(); ++n)
		ts << d->crls[n].toPEM();
	return true;
}

bool CertificateCollection::toPKCS7File(const QString &fileName, const QString &provider)
{
	CertCollectionContext *col = static_cast<CertCollectionContext *>(getContext("certcollection", provider));

	QList<CertContext*> cert_list;
	QList<CRLContext*> crl_list;
	int n;
	for(n = 0; n < d->certs.count(); ++n)
	{
		CertContext *c = static_cast<CertContext *>(d->certs[n].context());
		cert_list += c;
	}
	for(n = 0; n < d->crls.count(); ++n)
	{
		CRLContext *c = static_cast<CRLContext *>(d->crls[n].context());
		crl_list += c;
	}

	QByteArray result = col->toPKCS7(cert_list, crl_list);
	delete col;

	return arrayToFile(fileName, result);
}

CertificateCollection CertificateCollection::fromFlatTextFile(const QString &fileName, ConvertResult *result, const QString &provider)
{
	QFile f(fileName);
	if(!f.open(QFile::ReadOnly))
	{
		if(result)
			*result = ErrorFile;
		return CertificateCollection();
	}

	CertificateCollection certs;
	QTextStream ts(&f);
	while(1)
	{
		bool isCRL;
		QString pem = readNextPem(&ts, &isCRL);
		if(pem.isNull())
			break;
		if(isCRL)
		{
			CRL c = CRL::fromPEM(pem, 0, provider);
			if(!c.isNull())
				certs.addCRL(c);
		}
		else
		{
			Certificate c = Certificate::fromPEM(pem, 0, provider);
			if(!c.isNull())
				certs.addCertificate(c);
		}
	}

	if(result)
		*result = ConvertGood;

	return certs;
}

CertificateCollection CertificateCollection::fromPKCS7File(const QString &fileName, ConvertResult *result, const QString &provider)
{
	QByteArray der;
	if(!arrayFromFile(fileName, &der))
	{
		if(result)
			*result = ErrorFile;
		return CertificateCollection();
	}

	CertificateCollection certs;

	QList<CertContext*> cert_list;
	QList<CRLContext*> crl_list;
	CertCollectionContext *col = static_cast<CertCollectionContext *>(getContext("certcollection", provider));
	ConvertResult r = col->fromPKCS7(der, &cert_list, &crl_list);
	delete col;

	if(result)
		*result = r;
	if(r == ConvertGood)
	{
		int n;
		for(n = 0; n < cert_list.count(); ++n)
		{
			Certificate c;
			c.change(cert_list[n]);
			certs.addCertificate(c);
		}
		for(n = 0; n < crl_list.count(); ++n)
		{
			CRL c;
			c.change(crl_list[n]);
			certs.addCRL(c);
		}
	}
	return certs;
}

//----------------------------------------------------------------------------
// CertificateAuthority
//----------------------------------------------------------------------------
CertificateAuthority::CertificateAuthority(const Certificate &cert, const PrivateKey &key, const QString &provider)
:Algorithm("ca", provider)
{
	static_cast<CAContext *>(context())->setup(*(static_cast<const CertContext *>(cert.context())), *(static_cast<const PKeyContext *>(key.context())));
}

Certificate CertificateAuthority::certificate() const
{
	Certificate c;
	c.change(static_cast<const CAContext *>(context())->certificate());
	return c;
}

Certificate CertificateAuthority::signRequest(const CertificateRequest &req, const QDateTime &notValidAfter) const
{
	Certificate c;
	CertContext *cc = static_cast<const CAContext *>(context())->signRequest(*(static_cast<const CSRContext *>(req.context())), notValidAfter);
	if(cc)
		c.change(cc);
	return c;
}

CRL CertificateAuthority::createCRL(const QDateTime &nextUpdate) const
{
	CRL crl;
	CRLContext *cc = static_cast<const CAContext *>(context())->createCRL(nextUpdate);
	if(cc)
		crl.change(cc);
	return crl;
}

CRL CertificateAuthority::updateCRL(const CRL &crl, const QList<CRLEntry> &entries, const QDateTime &nextUpdate) const
{
	CRL new_crl;
	CRLContext *cc = static_cast<const CAContext *>(context())->updateCRL(*(static_cast<const CRLContext *>(crl.context())), entries, nextUpdate);
	if(cc)
		new_crl.change(cc);
	return new_crl;
}

//----------------------------------------------------------------------------
// KeyBundle
//----------------------------------------------------------------------------
class KeyBundle::Private : public QSharedData
{
public:
	QString name;
	CertificateChain chain;
	PrivateKey key;
};

KeyBundle::KeyBundle()
:d(new Private)
{
}

KeyBundle::KeyBundle(const QString &fileName, const QSecureArray &passphrase)
:d(new Private)
{
	*this = fromFile(fileName, passphrase, 0, QString());
}

KeyBundle::KeyBundle(const KeyBundle &from)
:d(from.d)
{
}

KeyBundle::~KeyBundle()
{
}

KeyBundle & KeyBundle::operator=(const KeyBundle &from)
{
	d = from.d;
	return *this;
}

bool KeyBundle::isNull() const
{
	return d->chain.isEmpty();
}

QString KeyBundle::name() const
{
	return d->name;
}

CertificateChain KeyBundle::certificateChain() const
{
	return d->chain;
}

PrivateKey KeyBundle::privateKey() const
{
	return d->key;
}

void KeyBundle::setName(const QString &s)
{
	d->name = s;
}

void KeyBundle::setCertificateChainAndKey(const CertificateChain &c, const PrivateKey &key)
{
	d->chain = c;
	d->key = key;
}

QByteArray KeyBundle::toArray(const QSecureArray &passphrase, const QString &provider) const
{
	PIXContext *pix = static_cast<PIXContext *>(getContext("pix", provider));

	QList<const CertContext*> list;
	for(int n = 0; n < d->chain.count(); ++n)
		list.append(static_cast<const CertContext *>(d->chain[n].context()));
	QByteArray buf = pix->toPKCS12(d->name, list, *(static_cast<const PKeyContext *>(d->key.context())), passphrase);
	delete pix;

	return buf;
}

bool KeyBundle::toFile(const QString &fileName, const QSecureArray &passphrase, const QString &provider) const
{
	return arrayToFile(fileName, toArray(passphrase, provider));
}

KeyBundle KeyBundle::fromArray(const QByteArray &a, const QSecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	QString name;
	QList<CertContext *> list;
	PKeyContext *kc = 0;

	KeyBundle bundle;
	PIXContext *pix = static_cast<PIXContext *>(getContext("pix", provider));
	ConvertResult r = pix->fromPKCS12(a, passphrase, &name, &list, &kc);

	// error converting without passphrase?  maybe a passphrase is needed
	// FIXME: we should only do this if we get ErrorPassphrase?
	if(r != ConvertGood && passphrase.isEmpty())
	{
		QSecureArray pass;
		if(ask_passphrase(QString(), 0, &pass))
			r = pix->fromPKCS12(a, pass, &name, &list, &kc);
	}

	if(result)
		*result = r;

	if(r == ConvertGood)
	{
		bundle.d->name = name;
		for(int n = 0; n < list.count(); ++n)
		{
			Certificate cert;
			cert.change(list[n]);
			bundle.d->chain.append(cert);
		}
		bundle.d->key.change(kc);
	}
	return bundle;
}

KeyBundle KeyBundle::fromFile(const QString &fileName, const QSecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	QByteArray der;
	if(!arrayFromFile(fileName, &der))
	{
		if(result)
			*result = ErrorFile;
		return KeyBundle();
	}
	return fromArray(der, passphrase, result, provider);
}

//----------------------------------------------------------------------------
// PGPKey
//----------------------------------------------------------------------------
PGPKey::PGPKey()
{
}

PGPKey::PGPKey(const QString &fileName)
{
	*this = fromFile(fileName, 0, QString());
}

PGPKey::PGPKey(const PGPKey &from)
:Algorithm(from)
{
}

PGPKey::~PGPKey()
{
}

PGPKey & PGPKey::operator=(const PGPKey &from)
{
	Algorithm::operator=(from);
	return *this;
}

bool PGPKey::isNull() const
{
	return (!context() ? true : false);
}

QString PGPKey::keyId() const
{
	return static_cast<const PGPKeyContext *>(context())->props()->keyId;
}

QString PGPKey::primaryUserId() const
{
	return static_cast<const PGPKeyContext *>(context())->props()->userIds.first();
}

QStringList PGPKey::userIds() const
{
	return static_cast<const PGPKeyContext *>(context())->props()->userIds;
}

bool PGPKey::isSecret() const
{
	return static_cast<const PGPKeyContext *>(context())->props()->isSecret;
}

QDateTime PGPKey::creationDate() const
{
	return static_cast<const PGPKeyContext *>(context())->props()->creationDate;
}

QDateTime PGPKey::expirationDate() const
{
	return static_cast<const PGPKeyContext *>(context())->props()->expirationDate;
}

QString PGPKey::fingerprint() const
{
	return static_cast<const PGPKeyContext *>(context())->props()->fingerprint;
}

bool PGPKey::inKeyring() const
{
	return static_cast<const PGPKeyContext *>(context())->props()->inKeyring;
}

bool PGPKey::isTrusted() const
{
	return static_cast<const PGPKeyContext *>(context())->props()->isTrusted;
}

QSecureArray PGPKey::toArray() const
{
	return static_cast<const PGPKeyContext *>(context())->toBinary();
}

QString PGPKey::toString() const
{
	return static_cast<const PGPKeyContext *>(context())->toAscii();
}

bool PGPKey::toFile(const QString &fileName) const
{
	return stringToFile(fileName, toString());
}

PGPKey PGPKey::fromArray(const QSecureArray &a, ConvertResult *result, const QString &provider)
{
	PGPKey k;
	PGPKeyContext *kc = static_cast<PGPKeyContext *>(getContext("pgpkey", provider));
	ConvertResult r = kc->fromBinary(a);
	if(result)
		*result = r;
	if(r == ConvertGood)
		k.change(kc);
	return k;
}

PGPKey PGPKey::fromString(const QString &s, ConvertResult *result, const QString &provider)
{
	PGPKey k;
	PGPKeyContext *kc = static_cast<PGPKeyContext *>(getContext("pgpkey", provider));
	ConvertResult r = kc->fromAscii(s);
	if(result)
		*result = r;
	if(r == ConvertGood)
		k.change(kc);
	return k;
}

PGPKey PGPKey::fromFile(const QString &fileName, ConvertResult *result, const QString &provider)
{
	QString str;
	if(!stringFromFile(fileName, &str))
	{
		if(result)
			*result = ErrorFile;
		return PGPKey();
	}
	return fromString(str, result, provider);
}

}
