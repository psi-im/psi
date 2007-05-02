/*
 * Copyright (C) 2003-2007  Justin Karneges <justin@affinix.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "qca_cert.h"

#include "qca_publickey.h"
#include "qcaprovider.h"

#include <QTextStream>
#include <QFile>

namespace QCA {

Provider::Context *getContext(const QString &type, const QString &provider);
Provider::Context *getContext(const QString &type, Provider *p);

// from qca_publickey.cpp
bool stringToFile(const QString &fileName, const QString &content);
bool stringFromFile(const QString &fileName, QString *s);
bool arrayToFile(const QString &fileName, const QByteArray &content);
bool arrayFromFile(const QString &fileName, QByteArray *a);
bool ask_passphrase(const QString &fname, void *ptr, SecureArray *answer);
ProviderList allProviders();
Provider *providerForName(const QString &name);
bool use_asker_fallback(ConvertResult r);

static CertificateInfo orderedToMap(const CertificateInfoOrdered &info)
{
	CertificateInfo out;

	// first, do all but EmailLegacy
	for(int n = 0; n < info.count(); ++n)
	{
		const CertificateInfoPair &i = info[n];
		if(i.type() != OtherInfoType && i.type() != EmailLegacy)
			out.insert(i.type(), i.value());
	}

	// lastly, apply EmailLegacy
	for(int n = 0; n < info.count(); ++n)
	{
		const CertificateInfoPair &i = info[n];
		if(i.type() == EmailLegacy)
		{
			// de-dup
			QList<QString> emails = out.values(Email);
			if(!emails.contains(i.value()))
				out.insert(Email, i.value());
		}
	}

	return out;
}

static void moveMapValues(CertificateInfo *from, CertificateInfoOrdered *to, CertificateInfoType type)
{
	QList<QString> values = from->values(type);
	from->remove(type);

	// multimap values are stored in reverse.  we'll insert backwards in
	//   order to right them.
	for(int n = values.count() - 1; n >= 0; --n)
		to->append(CertificateInfoPair(type, values[n]));
}

static CertificateInfoOrdered mapToOrdered(const CertificateInfo &info)
{
	CertificateInfo in = info;
	CertificateInfoOrdered out;

	// have a specific order for some types
	moveMapValues(&in, &out, CommonName);
	moveMapValues(&in, &out, Country);
	moveMapValues(&in, &out, Locality);
	moveMapValues(&in, &out, State);
	moveMapValues(&in, &out, Organization);
	moveMapValues(&in, &out, OrganizationalUnit);
	moveMapValues(&in, &out, Email);
	moveMapValues(&in, &out, URI);
	moveMapValues(&in, &out, DNS);
	moveMapValues(&in, &out, IPAddress);
	moveMapValues(&in, &out, XMPP);

	// get remaining types
	QList<CertificateInfoType> typesLeft = in.keys();
	typesLeft.removeAll(OtherInfoType);

	// dedup
	QList<CertificateInfoType> types;
	for(int n = 0; n < typesLeft.count(); ++n)
	{
		if(!types.contains(typesLeft[n]))
			types += typesLeft[n];
	}

	// insert the rest of the types in the order we got them (map order)
	for(int n = 0; n < types.count(); ++n)
		moveMapValues(&in, &out, types[n]);

	Q_ASSERT(in.isEmpty());

	return out;
}

//----------------------------------------------------------------------------
// Global
//----------------------------------------------------------------------------
static const char *shortNameByType(CertificateInfoType type)
{
	switch(type)
	{
		case CommonName:         return "CN";
		case Locality:           return "L";
		case State:              return "ST";
		case Organization:       return "O";
		case OrganizationalUnit: return "OU";
		case Country:            return "C";
		case EmailLegacy:        return "emailAddress";
		default:                 break;
	}
	return 0;
}

static const char *oidByDNType(CertificateInfoType type)
{
	switch(type)
	{
		case CommonName:            break;
		case EmailLegacy:           return "1.2.840.113549.1.9.1";
		case Organization:          break;
		case OrganizationalUnit:    break;
		case Locality:              break;
		case IncorporationLocality: return "1.3.6.1.4.1.311.60.2.1.1";
		case State:                 break;
		case IncorporationState:    return "1.3.6.1.4.1.311.60.2.1.2";
		case Country:               break;
		case IncorporationCountry:  return "1.3.6.1.4.1.311.60.2.1.3";
		default:                    break;
	}
	return 0;
}

static QString knownDNLabel(CertificateInfoType type)
{
	const char *str = shortNameByType(type);
	if(str)
		return str;

	str = oidByDNType(type);
	if(str)
		return QString("OID.") + str;

	return QString();
}

QString orderedToDNString(const CertificateInfoOrdered &in)
{
	QStringList parts;
	foreach(const CertificateInfoPair &i, in)
	{
		if(i.section() != CertificateInfoPair::DN)
			continue;

		QString name = knownDNLabel(i.type());
		if(name.isEmpty())
			name = QString("OID.") + i.oid();

		parts += name + '=' + i.value();
	}
	return parts.join(", ");
}

CertificateInfoOrdered orderedDNOnly(const CertificateInfoOrdered &in)
{
	CertificateInfoOrdered out;
	for(int n = 0; n < in.count(); ++n)
	{
		if(in[n].section() == CertificateInfoPair::DN)
			out += in[n];
	}
	return out;
}

static QString baseCertName(const CertificateInfo &info)
{
	QString str = info.value(CommonName);
	if(str.isEmpty())
	{
		str = info.value(Organization);
		if(str.isEmpty())
			str = "Unnamed";
	}
	return str;
}

static QList<int> findSameName(const QString &name, const QStringList &list)
{
	QList<int> out;
	for(int n = 0; n < list.count(); ++n)
	{
		if(list[n] == name)
			out += n;
	}
	return out;
}

static QString uniqueSubjectValue(CertificateInfoType type, const QList<int> items, const QList<Certificate> &certs, int i)
{
	QStringList vals = certs[items[i]].subjectInfo().values(type);
	if(!vals.isEmpty())
	{
		foreach(int n, items)
		{
			if(n == items[i])
				continue;

			QStringList other_vals = certs[n].subjectInfo().values(type);
			for(int k = 0; k < vals.count(); ++k)
			{
				if(other_vals.contains(vals[k]))
				{
					vals.removeAt(k);
					break;
				}
			}

			if(vals.isEmpty())
				break;
		}

		if(!vals.isEmpty())
			return vals[0];
	}

	return QString();
}

static QString uniqueIssuerName(const QList<int> items, const QList<Certificate> &certs, int i)
{
	QString val = baseCertName(certs[items[i]].issuerInfo());

	bool found = false;
	foreach(int n, items)
	{
		if(n == items[i])
			continue;

		QString other_val = baseCertName(certs[n].issuerInfo());
		if(other_val == val)
		{
			found = true;
			break;
		}
	}

	if(!found)
		return val;

	return QString();
}

static const char *constraintToString(ConstraintType type)
{
	switch(type)
	{
		case DigitalSignature:   return "DigitalSignature";
		case NonRepudiation:     return "NonRepudiation";
		case KeyEncipherment:    return "KeyEncipherment";
		case DataEncipherment:   return "DataEncipherment";
		case KeyAgreement:       return "KeyAgreement";
		case KeyCertificateSign: return "KeyCertificateSign";
		case CRLSign:            return "CRLSign";
		case EncipherOnly:       return "EncipherOnly";
		case DecipherOnly:       return "DecipherOnly";
		case ServerAuth:         return "ServerAuth";
		case ClientAuth:         return "ClientAuth";
		case CodeSigning:        return "CodeSigning";
		case EmailProtection:    return "EmailProtection";
		case IPSecEndSystem:     return "IPSecEndSystem";
		case IPSecTunnel:        return "IPSecTunnel";
		case IPSecUser:          return "IPSecUser";
		case TimeStamping:       return "TimeStamping";
		case OCSPSigning:        return "OCSPSigning";
	}
	return 0;
}

static QString uniqueConstraintValue(ConstraintType type, const QList<int> items, const QList<Certificate> &certs, int i)
{
	ConstraintType val = type;
	if(certs[items[i]].constraints().contains(type))
	{
		bool found = false;
		foreach(int n, items)
		{
			if(n == items[i])
				continue;

			Constraints other_vals = certs[n].constraints();
			if(other_vals.contains(val))
			{
				found = true;
				break;
			}
		}

		if(!found)
			return QString(constraintToString(val));
	}

	return QString();
}

static QString makeUniqueName(const QList<int> &items, const QStringList &list, const QList<Certificate> &certs, int i)
{
	QString str, name;

	// different organization?
	str = uniqueSubjectValue(Organization, items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" of ") + str;
		goto end;
	}

	// different organizational unit?
	str = uniqueSubjectValue(OrganizationalUnit, items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" of ") + str;
		goto end;
	}

	// different email address?
	str = uniqueSubjectValue(Email, items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" <") + str + '>';
		goto end;
	}

	// different xmpp addresses?
	str = uniqueSubjectValue(XMPP, items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" <xmpp:") + str + '>';
		goto end;
	}

	// different issuers?
	str = uniqueIssuerName(items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" by ") + str;
		goto end;
	}

	// different usages?

	// DigitalSignature
	str = uniqueConstraintValue(DigitalSignature, items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" for ") + str;
		goto end;
	}

	// ClientAuth
	str = uniqueConstraintValue(ClientAuth, items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" for ") + str;
		goto end;
	}

	// EmailProtection
	str = uniqueConstraintValue(EmailProtection, items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" for ") + str;
		goto end;
	}

	// DataEncipherment
	str = uniqueConstraintValue(DataEncipherment, items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" for ") + str;
		goto end;
	}

	// EncipherOnly
	str = uniqueConstraintValue(EncipherOnly, items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" for ") + str;
		goto end;
	}

	// DecipherOnly
	str = uniqueConstraintValue(DecipherOnly, items, certs, i);
	if(!str.isEmpty())
	{
		name = list[items[i]] + QString(" for ") + str;
		goto end;
	}

	// if there's nothing easily unique, then do a DN string
	name = certs[items[i]].subjectInfoOrdered().toString();

end:
	return name;
}

QStringList makeFriendlyNames(const QList<Certificate> &list)
{
	QStringList names;

	// give a base name to all certs first
	foreach(const Certificate &cert, list)
		names += baseCertName(cert.subjectInfo());

	// come up with a collision list
	QList< QList<int> > itemCollisions;
	foreach(const QString &name, names)
	{
		// anyone else using this name?
		QList<int> items = findSameName(name, names);
		if(items.count() > 1)
		{
			// don't save duplicate collisions
			bool haveAlready = false;
			foreach(const QList<int> &other, itemCollisions)
			{
				foreach(int n, items)
				{
					if(other.contains(n))
					{
						haveAlready = true;
						break;
					}
				}

				if(haveAlready)
					break;
			}

			if(haveAlready)
				continue;

			itemCollisions += items;
		}
	}

	// resolve collisions by providing extra details
	foreach(const QList<int> &items, itemCollisions)
	{
		//printf("%d items are using [%s]\n", items.count(), qPrintable(names[items[0]]));

		for(int n = 0; n < items.count(); ++n)
		{
			names[items[n]] = makeUniqueName(items, names, list, n);
			//printf("  %d: reassigning: [%s]\n", items[n], qPrintable(names[items[n]]));
		}
	}

	return names;
}

//----------------------------------------------------------------------------
// CertificateInfoPair
//----------------------------------------------------------------------------
class CertificateInfoPair::Private : public QSharedData
{
public:
	bool use_altname;
	CertificateInfoType type;
	QString oid;
	QString value;

	Private()
	{
		use_altname = false;
		type = (CertificateInfoType)-1;
	}
};

CertificateInfoPair::CertificateInfoPair()
:d(new Private)
{
}

CertificateInfoPair::CertificateInfoPair(CertificateInfoType type, const QString &value)
:d(new Private)
{
	switch(type)
	{
		case CommonName:
		case EmailLegacy:
		case Organization:
		case OrganizationalUnit:
		case Locality:
		case IncorporationLocality:
		case State:
		case IncorporationState:
		case Country:
		case IncorporationCountry:
			d->use_altname = false;
			break;
		default:
			d->use_altname = true;
			break;
	}

	d->type = type;
	d->value = value;
}

CertificateInfoPair::CertificateInfoPair(const QString &oid, const QString &value, Section section)
:d(new Private)
{
	if(section == AltName)
		d->use_altname = true;
	else
		d->use_altname = false;

	d->type = OtherInfoType;
	d->oid = oid;
	d->value = value;
}

CertificateInfoPair::CertificateInfoPair(const CertificateInfoPair &from)
:d(from.d)
{
}

CertificateInfoPair::~CertificateInfoPair()
{
}

CertificateInfoPair & CertificateInfoPair::operator=(const CertificateInfoPair &from)
{
	d = from.d;
	return *this;
}

CertificateInfoPair::Section CertificateInfoPair::section() const
{
	if(d->use_altname)
		return AltName;
	else
		return DN;
}

CertificateInfoType CertificateInfoPair::type() const
{
	return d->type;
}

QString CertificateInfoPair::oid() const
{
	return d->oid;
}

QString CertificateInfoPair::value() const
{
	return d->value;
}

bool CertificateInfoPair::operator==(const CertificateInfoPair &other) const
{
	if((d->type == other.d->type || (d->use_altname == other.d->use_altname && d->oid == other.d->oid)) && d->value == other.d->value)
		return true;
	return false;
}

//----------------------------------------------------------------------------
// CertificateOptions
//----------------------------------------------------------------------------
class CertificateOptions::Private
{
public:
	CertificateRequestFormat format;

	QString challenge;
	CertificateInfoOrdered info;
	CertificateInfo infoMap;
	Constraints constraints;
	QStringList policies;
	QStringList crlLocations;
	bool isCA;
	int pathLimit;
	BigInteger serial;
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
	if(d->infoMap.value(CommonName).isEmpty() || d->infoMap.value(Country).isEmpty())
		return false;
	if(d->infoMap.value(Country).length() != 2)
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
	return d->infoMap;
}

CertificateInfoOrdered CertificateOptions::infoOrdered() const
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

QStringList CertificateOptions::crlLocations() const
{
	return d->crlLocations;
}

bool CertificateOptions::isCA() const
{
	return d->isCA;
}

int CertificateOptions::pathLimit() const
{
	return d->pathLimit;
}

BigInteger CertificateOptions::serialNumber() const
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
	d->info = mapToOrdered(info);
	d->infoMap = info;
}

void CertificateOptions::setInfoOrdered(const CertificateInfoOrdered &info)
{
	d->info = info;
	d->infoMap = orderedToMap(info);
}

void CertificateOptions::setConstraints(const Constraints &constraints)
{
	d->constraints = constraints;
}

void CertificateOptions::setPolicies(const QStringList &policies)
{
	d->policies = policies;
}

void CertificateOptions::setCRLLocations(const QStringList &locations)
{
	d->crlLocations = locations;
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

void CertificateOptions::setSerialNumber(const BigInteger &i)
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

class Certificate::Private : public QSharedData
{
public:
	CertificateInfo subjectInfoMap, issuerInfoMap;

	void update(CertContext *c)
	{
		if(c)
		{
			subjectInfoMap = orderedToMap(c->props()->subject);
			issuerInfoMap = orderedToMap(c->props()->issuer);
		}
		else
		{
			subjectInfoMap = CertificateInfo();
			issuerInfoMap = CertificateInfo();
		}
	}
};

Certificate::Certificate()
:d(new Private)
{
}

Certificate::Certificate(const QString &fileName)
:d(new Private)
{
	*this = fromPEMFile(fileName, 0, QString());
}

Certificate::Certificate(const CertificateOptions &opts, const PrivateKey &key, const QString &provider)
:d(new Private)
{
	CertContext *c = static_cast<CertContext *>(getContext("cert", provider));
	if(c->createSelfSigned(opts, *(static_cast<const PKeyContext *>(key.context()))))
		change(c);
	else
		delete c;
}

Certificate::Certificate(const Certificate &from)
:Algorithm(from), d(from.d)
{
}

Certificate::~Certificate()
{
}

Certificate & Certificate::operator=(const Certificate &from)
{
	Algorithm::operator=(from);
	d = from.d;
	return *this;
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
	return d->subjectInfoMap;
}

CertificateInfoOrdered Certificate::subjectInfoOrdered() const
{
	return static_cast<const CertContext *>(context())->props()->subject;
}

CertificateInfo Certificate::issuerInfo() const
{
	return d->issuerInfoMap;
}

CertificateInfoOrdered Certificate::issuerInfoOrdered() const
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

QStringList Certificate::crlLocations() const
{
	return static_cast<const CertContext *>(context())->props()->crlLocations;
}

QString Certificate::commonName() const
{
	return d->subjectInfoMap.value(CommonName);
}

BigInteger Certificate::serialNumber() const
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

SecureArray Certificate::toDER() const
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

Certificate Certificate::fromDER(const SecureArray &a, ConvertResult *result, const QString &provider)
{
	Certificate c;
	CertContext *cc = static_cast<CertContext *>(getContext("cert", provider));
	ConvertResult r = cc->fromDER(a);
	if(result)
		*result = r;
	if(r == ConvertGood)
		c.change(cc);
	else
		delete cc;
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
	else
		delete cc;
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
	foreach( const QString &commonName, subjectInfo().values(CommonName) ) {
		if (certnameMatchesAddress(commonName, peerHost))
			return true;
	}

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

void Certificate::change(CertContext *c)
{
	Algorithm::change(c);
	d->update(static_cast<CertContext *>(context()));
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
class CertificateRequest::Private : public QSharedData
{
public:
	CertificateInfo subjectInfoMap;

	void update(CSRContext *c)
	{
		if(c)
			subjectInfoMap = orderedToMap(c->props()->subject);
		else
			subjectInfoMap = CertificateInfo();
	}
};

CertificateRequest::CertificateRequest()
:d(new Private)
{
}

CertificateRequest::CertificateRequest(const QString &fileName)
:d(new Private)
{
	*this = fromPEMFile(fileName, 0, QString());
}

CertificateRequest::CertificateRequest(const CertificateOptions &opts, const PrivateKey &key, const QString &provider)
:d(new Private)
{
	CSRContext *c = static_cast<CSRContext *>(getContext("csr", provider));
	if(c->createRequest(opts, *(static_cast<const PKeyContext *>(key.context()))))
		change(c);
	else
		delete c;
}

CertificateRequest::CertificateRequest(const CertificateRequest &from)
:Algorithm(from), d(from.d)
{
}

CertificateRequest::~CertificateRequest()
{
}

CertificateRequest & CertificateRequest::operator=(const CertificateRequest &from)
{
	Algorithm::operator=(from);
	d = from.d;
	return *this;
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
	return d->subjectInfoMap;
}

CertificateInfoOrdered CertificateRequest::subjectInfoOrdered() const
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

SecureArray CertificateRequest::toDER() const
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

CertificateRequest CertificateRequest::fromDER(const SecureArray &a, ConvertResult *result, const QString &provider)
{
	CertificateRequest c;
	CSRContext *csr = static_cast<CSRContext *>(getContext("csr", provider));
	ConvertResult r = csr->fromDER(a);
	if(result)
		*result = r;
	if(r == ConvertGood)
		c.change(csr);
	else
		delete csr;
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
	else
		delete csr;
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
	else
		delete csr;
	return c;
}

void CertificateRequest::change(CSRContext *c)
{
	Algorithm::change(c);
	d->update(static_cast<CSRContext *>(context()));
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

CRLEntry::CRLEntry(const BigInteger serial, const QDateTime time, Reason r)
{
	_serial = serial;
	_time = time;
	_reason = r;
}

BigInteger CRLEntry::serialNumber() const
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
class CRL::Private : public QSharedData
{
public:
	CertificateInfo issuerInfoMap;

	void update(CRLContext *c)
	{
		if(c)
			issuerInfoMap = orderedToMap(c->props()->issuer);
		else
			issuerInfoMap = CertificateInfo();
	}
};

CRL::CRL()
:d(new Private)
{
}

CRL::CRL(const CRL &from)
:Algorithm(from), d(from.d)
{
}

CRL::~CRL()
{
}

CRL & CRL::operator=(const CRL &from)
{
	Algorithm::operator=(from);
	d = from.d;
	return *this;
}

bool CRL::isNull() const
{
	return (!context() ? true : false);
}

CertificateInfo CRL::issuerInfo() const
{
	return d->issuerInfoMap;
}

CertificateInfoOrdered CRL::issuerInfoOrdered() const
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

SecureArray CRL::signature() const
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

SecureArray CRL::toDER() const
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

CRL CRL::fromDER(const SecureArray &a, ConvertResult *result, const QString &provider)
{
	CRL c;
	CRLContext *cc = static_cast<CRLContext *>(getContext("crl", provider));
	ConvertResult r = cc->fromDER(a);
	if(result)
		*result = r;
	if(r == ConvertGood)
		c.change(cc);
	else
		delete cc;
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
	else
		delete cc;
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

void CRL::change(CRLContext *c)
{
	Algorithm::change(c);
	d->update(static_cast<CRLContext *>(context()));
}

//----------------------------------------------------------------------------
// Store
//----------------------------------------------------------------------------
// CRL / X509 CRL
// CERTIFICATE / X509 CERTIFICATE
static QString readNextPem(QTextStream *ts, bool *isCRL)
{
	QString pem;
	bool crl = false;
	bool found = false;
	bool done = false;
	while(!ts->atEnd())
	{
		QString line = ts->readLine();
		if(!found)
		{
			if(line.startsWith("-----BEGIN "))
			{
				if(line.contains("CERTIFICATE"))
				{
					found = true;
					pem += line + '\n';
					crl = false;
				}
				else if(line.contains("CRL"))
				{
					found = true;
					pem += line + '\n';
					crl = true;
				}
			}
		}
		else
		{
			pem += line + '\n';
			if(line.startsWith("-----END "))
			{
				done = true;
				break;
			}
		}
	}
	if(!done)
                return QString();
	if(isCRL)
		*isCRL = crl;
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
	return isSupported("certcollection", provider);
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
		bool isCRL = false;
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

KeyBundle::KeyBundle(const QString &fileName, const SecureArray &passphrase)
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

QByteArray KeyBundle::toArray(const SecureArray &passphrase, const QString &provider) const
{
	PKCS12Context *pix = static_cast<PKCS12Context *>(getContext("pkcs12", provider));

	QList<const CertContext*> list;
	for(int n = 0; n < d->chain.count(); ++n)
		list.append(static_cast<const CertContext *>(d->chain[n].context()));
	QByteArray buf = pix->toPKCS12(d->name, list, *(static_cast<const PKeyContext *>(d->key.context())), passphrase);
	delete pix;

	return buf;
}

bool KeyBundle::toFile(const QString &fileName, const SecureArray &passphrase, const QString &provider) const
{
	return arrayToFile(fileName, toArray(passphrase, provider));
}

KeyBundle KeyBundle::fromArray(const QByteArray &a, const SecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	QString name;
	QList<CertContext*> list;
	PKeyContext *kc = 0;

	KeyBundle bundle;
	PKCS12Context *pix = static_cast<PKCS12Context *>(getContext("pkcs12", provider));
	ConvertResult r = pix->fromPKCS12(a, passphrase, &name, &list, &kc);

	// error converting without passphrase?  maybe a passphrase is needed
	if(use_asker_fallback(r) && passphrase.isEmpty())
	{
		SecureArray pass;
		if(ask_passphrase(QString(), 0, &pass))
			r = pix->fromPKCS12(a, pass, &name, &list, &kc);
	}
	delete pix;

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

KeyBundle KeyBundle::fromFile(const QString &fileName, const SecureArray &passphrase, ConvertResult *result, const QString &provider)
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

SecureArray PGPKey::toArray() const
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

PGPKey PGPKey::fromArray(const SecureArray &a, ConvertResult *result, const QString &provider)
{
	PGPKey k;
	PGPKeyContext *kc = static_cast<PGPKeyContext *>(getContext("pgpkey", provider));
	ConvertResult r = kc->fromBinary(a);
	if(result)
		*result = r;
	if(r == ConvertGood)
		k.change(kc);
	else
		delete kc;
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
	else
		delete kc;
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

//----------------------------------------------------------------------------
// KeyLoader
//----------------------------------------------------------------------------
class KeyLoaderThread : public QThread
{
	Q_OBJECT
public:
	enum Type { PKPEMFile, PKPEM, PKDER, KBDERFile, KBDER };

	class In
	{
	public:
		Type type;
		QString fileName, pem;
		SecureArray der;
		QByteArray kbder;
	};

	class Out
	{
	public:
		ConvertResult convertResult;
		PrivateKey privateKey;
		KeyBundle keyBundle;
	};

	In in;
	Out out;

	KeyLoaderThread(QObject *parent = 0) : QThread(parent)
	{
	}

protected:
	virtual void run()
	{
		if(in.type == PKPEMFile)
			out.privateKey = PrivateKey::fromPEMFile(in.fileName, SecureArray(), &out.convertResult);
		else if(in.type == PKPEM)
			out.privateKey = PrivateKey::fromPEM(in.pem, SecureArray(), &out.convertResult);
		else if(in.type == PKDER)
			out.privateKey = PrivateKey::fromDER(in.der, SecureArray(), &out.convertResult);
		else if(in.type == KBDERFile)
			out.keyBundle = KeyBundle::fromFile(in.fileName, SecureArray(), &out.convertResult);
		else if(in.type == KBDER)
			out.keyBundle = KeyBundle::fromArray(in.kbder, SecureArray(), &out.convertResult);
	}
};

class KeyLoader::Private : public QObject
{
	Q_OBJECT
public:
	KeyLoader *q;

	bool active;
	KeyLoaderThread *thread;
	KeyLoaderThread::In in;
	KeyLoaderThread::Out out;

	Private(KeyLoader *_q) : QObject(_q), q(_q)
	{
		active = false;
	}

	void reset()
	{
		in = KeyLoaderThread::In();
		out = KeyLoaderThread::Out();
	}

	void start()
	{
		active = true;
		thread = new KeyLoaderThread(this);
		// used queued for signal-safety
		connect(thread, SIGNAL(finished()), SLOT(thread_finished()), Qt::QueuedConnection);
		thread->in = in;
		thread->start();
	}

private slots:
	void thread_finished()
	{
		out = thread->out;
		delete thread;
		thread = 0;
		active = false;

		emit q->finished();
	}
};

KeyLoader::KeyLoader(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

KeyLoader::~KeyLoader()
{
	delete d;
}

void KeyLoader::loadPrivateKeyFromPEMFile(const QString &fileName)
{
	Q_ASSERT(!d->active);
	if(d->active)
		return;

	d->reset();
	d->in.type = KeyLoaderThread::PKPEMFile;
	d->in.fileName = fileName;
	d->start();
}

void KeyLoader::loadPrivateKeyFromPEM(const QString &s)
{
	Q_ASSERT(!d->active);
	if(d->active)
		return;

	d->reset();
	d->in.type = KeyLoaderThread::PKPEM;
	d->in.pem = s;
	d->start();
}

void KeyLoader::loadPrivateKeyFromDER(const SecureArray &a)
{
	Q_ASSERT(!d->active);
	if(d->active)
		return;

	d->reset();
	d->in.type = KeyLoaderThread::PKDER;
	d->in.der = a;
	d->start();
}

void KeyLoader::loadKeyBundleFromFile(const QString &fileName)
{
	Q_ASSERT(!d->active);
	if(d->active)
		return;

	d->reset();
	d->in.type = KeyLoaderThread::KBDERFile;
	d->in.fileName = fileName;
	d->start();
}

void KeyLoader::loadKeyBundleFromArray(const QByteArray &a)
{
	Q_ASSERT(!d->active);
	if(d->active)
		return;

	d->reset();
	d->in.type = KeyLoaderThread::KBDERFile;
	d->in.kbder = a;
	d->start();
}

ConvertResult KeyLoader::convertResult() const
{
	return d->out.convertResult;
}

PrivateKey KeyLoader::privateKey() const
{
	return d->out.privateKey;
}

KeyBundle KeyLoader::keyBundle() const
{
	return d->out.keyBundle;
}

}

#include "qca_cert.moc"
