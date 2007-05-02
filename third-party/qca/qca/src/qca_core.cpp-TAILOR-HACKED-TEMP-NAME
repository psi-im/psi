/*
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
 * Copyright (C) 2004,2005  Brad Hards <bradh@frogmouth.net>
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

#include "qca_core.h"

#include <QtCore>
#include "qca_plugin.h"
#include "qca_textfilter.h"
#include "qca_cert.h"
#include "qca_keystore.h"
#include "qcaprovider.h"

#ifdef Q_OS_UNIX
# include <unistd.h>
#endif

int qcaVersion()
{
	return QCA_VERSION;
}

namespace QCA {

// from qca_tools
bool botan_init(int prealloc, bool mmap);
void botan_deinit();

// from qca_default
Provider *create_default_provider();

//----------------------------------------------------------------------------
// Global
//----------------------------------------------------------------------------
class Global
{
public:
	int refs;
	bool secmem;
	bool first_scan;
	QString app_name;
	ProviderManager manager;
	Random *rng;
	Logger *logger;
	QVariantMap properties;
	QMap<QString,QVariantMap> config;

	Global()
	{
		refs = 0;
		secmem = false;
		first_scan = false;
		rng = 0;
		logger = new Logger;
	}

	~Global()
	{
		KeyStoreManager::shutdown();
		delete logger;
		delete rng;
	}

	void ensure_first_scan()
	{
		if(!first_scan)
			scan();
	}

	void scan()
	{
		first_scan = true;
		manager.scan();
	}

	void ksm_scan()
	{
		KeyStoreManager::scan();
	}
};

Q_GLOBAL_STATIC(QMutex, global_mutex)
static Global *global = 0;

static bool features_have(const QStringList &have, const QStringList &want)
{
	for(QStringList::ConstIterator it = want.begin(); it != want.end(); ++it)
	{
		if(!have.contains(*it))
			return false;
	}
	return true;
}

void init(MemoryMode mode, int prealloc)
{
	QMutexLocker locker(global_mutex());
	if(global)
	{
		++(global->refs);
		return;
	}

	bool allow_mmap_fallback = false;
	bool drop_root = false;
	if(mode == Practical)
	{
		allow_mmap_fallback = true;
		drop_root = true;
	}
	else if(mode == Locking)
		drop_root = true;

	bool secmem = botan_init(prealloc, allow_mmap_fallback);

	if(drop_root)
	{
#ifdef Q_OS_UNIX
		setuid(getuid());
#endif
	}

	global = new Global;
	global->secmem = secmem;
	global->manager.setDefault(create_default_provider()); // manager owns it
	++(global->refs);

	// for maximum setuid safety, qca should be initialized before qapp:
	//
	//   int main(int argc, char **argv)
	//   {
	//       QCA::Initializer init;
	//       QCoreApplication app(argc, argv);
	//       return 0;
	//   }
	//
	// however, the above code has the unfortunate side-effect of causing
	// qapp to deinit before qca, which can cause problems with any
	// plugins that have active objects (notably KeyStore).  we'll use a
	// post routine to force qca to deinit first.
	qAddPostRoutine(deinit);
}

void init()
{
	init(Practical, 64);
}

void deinit()
{
	QMutexLocker locker(global_mutex());
	if(!global)
		return;
	--(global->refs);
	if(global->refs == 0)
	{
		delete global;
		global = 0;
		botan_deinit();
	}
}

bool haveSecureMemory()
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return false;

	return global->secmem;
}

bool isSupported(const QStringList &features, const QString &provider)
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return false;

	// single
	if(!provider.isEmpty())
	{
		Provider *p = global->manager.find(provider);
		if(!p)
		{
			// ok, try scanning for new stuff
			global->scan();
			p = global->manager.find(provider);
		}

		if(p && features_have(p->features(), features))
			return true;
	}
	// all
	else
	{
		if(features_have(global->manager.allFeatures(), features))
			return true;

		// ok, try scanning for new stuff
		global->scan();

		if(features_have(global->manager.allFeatures(), features))
			return true;
	}
	return false;
}

bool isSupported(const char *features, const QString &provider)
{
	return isSupported(QString(features).split(',', QString::SkipEmptyParts), provider);
}

QStringList supportedFeatures()
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return QStringList();

	// query all features
	global->scan();
	return global->manager.allFeatures();
}

QStringList defaultFeatures()
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return QStringList();

	return global->manager.find("default")->features();
}

ProviderList providers()
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return ProviderList();

	global->ensure_first_scan();

	return global->manager.providers();
}

bool insertProvider(Provider *p, int priority)
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return false;

	global->ensure_first_scan();

	return global->manager.add(p, priority);
}

void setProviderPriority(const QString &name, int priority)
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return;

	global->ensure_first_scan();

	global->manager.changePriority(name, priority);
}

int providerPriority(const QString &name)
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return -1;

	global->ensure_first_scan();

	return global->manager.getPriority(name);
}

Provider *findProvider(const QString &name)
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return 0;

	global->ensure_first_scan();

	return global->manager.find(name);
}

Provider *defaultProvider()
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return 0;

	return global->manager.find("default");
}

void scanForPlugins()
{
	{
		QMutexLocker locker(global_mutex());
		Q_ASSERT(global);
		if(!global)
			return;

		global->scan();
	}
	global->ksm_scan();
}

void unloadAllPlugins()
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return;

	// if the global_rng was owned by a plugin, then delete it
	if(global->rng && (global->rng->provider() != global->manager.find("default")))
	{
		delete global->rng;
		global->rng = 0;
	}

	global->manager.unloadAll();
}

QString pluginDiagnosticText()
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return QString();

	return global->manager.diagnosticText();
}

void clearPluginDiagnosticText()
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return;

	global->manager.clearDiagnosticText();
}

void setProperty(const QString &name, const QVariant &value)
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return;

	global->properties[name] = value;
}

QVariant getProperty(const QString &name)
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return QVariant();

	return global->properties.value(name);
}

static bool configIsValid(const QVariantMap &config)
{
	if(!config.contains("formtype"))
		return false;
	QMapIterator<QString,QVariant> it(config);
	while(it.hasNext())
	{
		it.next();
		const QVariant &v = it.value();
		if(v.type() != QVariant::String && v.type() != QVariant::Int && v.type() != QVariant::Bool)
			return false;
	}
	return true;
}

static QVariantMap readConfig(const QString &name)
{
	QSettings settings("Affinix", "QCA");
	settings.beginGroup("ProviderConfig");
	QStringList providerNames = settings.value("providerNames").toStringList();
	if(!providerNames.contains(name))
		return QVariantMap();

	settings.beginGroup(name);
	QStringList keys = settings.childKeys();
	QVariantMap map;
	foreach(QString key, keys)
		map[key] = settings.value(key);
	settings.endGroup();

	if(!configIsValid(map))
		return QVariantMap();
	return map;
}

static bool writeConfig(const QString &name, const QVariantMap &config, bool systemWide = false)
{
	QSettings settings(QSettings::NativeFormat, systemWide ? QSettings::SystemScope : QSettings::UserScope, "Affinix", "QCA");
	settings.beginGroup("ProviderConfig");

	// version
	settings.setValue("version", 2);

	// add the entry if needed
	QStringList providerNames = settings.value("providerNames").toStringList();
	if(!providerNames.contains(name))
		providerNames += name;
	settings.setValue("providerNames", providerNames);

	settings.beginGroup(name);
	QMapIterator<QString,QVariant> it(config);
	while(it.hasNext())
	{
		it.next();
		settings.setValue(it.key(), it.value());
	}
	settings.endGroup();

	if(settings.status() == QSettings::NoError)
		return true;
	return false;
}

void setProviderConfig(const QString &name, const QVariantMap &config)
{
	if(!configIsValid(config))
		return;

	{
		QMutexLocker locker(global_mutex());
		Q_ASSERT(global);
		if(!global)
			return;

		global->config[name] = config;
	}

	Provider *p = findProvider(name);
	if(p)
		p->configChanged(config);
}

QVariantMap getProviderConfig(const QString &name)
{
	QVariantMap conf;

	{
		QMutexLocker locker(global_mutex());
		Q_ASSERT(global);
		if(!global)
			return QVariantMap();

		// try loading from persistent storage
		conf = readConfig(name);

		// if not, load the one from memory
		if(conf.isEmpty())
			conf = global->config.value(name);
	}

	// if provider doesn't exist or doesn't have a valid config form,
	//   use the config we loaded
	Provider *p = findProvider(name);
	if(!p)
		return conf;
	QVariantMap pconf = p->defaultConfig();
	if(!configIsValid(pconf))
		return conf;

	// if the config loaded was empty, use the provider's config
	if(conf.isEmpty())
		return pconf;

	// if the config formtype doesn't match the provider's formtype,
	//   then use the provider's
	if(pconf["formtype"] != conf["formtype"])
		return pconf;

	// otherwise, use the config loaded
	return conf;
}

void saveProviderConfig(const QString &name)
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return;

	QVariantMap conf = global->config.value(name);
	if(conf.isEmpty())
		return;

	writeConfig(name, conf);
}

Random & globalRNG()
{
	{
		QMutexLocker locker(global_mutex());
		Q_ASSERT(global);
	}

	if(!global->rng)
		global->rng = new Random;
	return *global->rng;
}

void setGlobalRNG(const QString &provider)
{
	{
		QMutexLocker locker(global_mutex());
		Q_ASSERT(global);
	}

	delete global->rng;
	global->rng = new Random(provider);
}

Logger *logger()
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);

	return global->logger;
}

void logText( const QString &message, Logger::Severity severity )
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);

        global->logger->logTextMessage( message, severity );
}

bool haveSystemStore()
{
	// ensure the system store is loaded
	KeyStoreManager::start("default");
	KeyStoreManager ksm;
	ksm.waitForBusyFinished();

	QStringList list = ksm.keyStores();
	for(int n = 0; n < list.count(); ++n)
	{
		KeyStore ks(list[n], &ksm);
		if(ks.type() == KeyStore::System && ks.holdsTrustedCertificates())
			return true;
	}
	return false;
}

CertificateCollection systemStore()
{
	// ensure the system store is loaded
	KeyStoreManager::start("default");
	KeyStoreManager ksm;
	ksm.waitForBusyFinished();

	CertificateCollection col;
	QStringList list = ksm.keyStores();
	for(int n = 0; n < list.count(); ++n)
	{
		KeyStore ks(list[n], &ksm);

		// system store
		if(ks.type() == KeyStore::System && ks.holdsTrustedCertificates())
		{
			// extract contents
			QList<KeyStoreEntry> entries = ks.entryList();
			for(int i = 0; i < entries.count(); ++i)
			{
				if(entries[i].type() == KeyStoreEntry::TypeCertificate)
					col.addCertificate(entries[i].certificate());
				else if(entries[i].type() == KeyStoreEntry::TypeCRL)
					col.addCRL(entries[i].crl());
			}
			break;
		}
	}
	return col;
}

QString appName()
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return QString();

	return global->app_name;
}

void setAppName(const QString &s)
{
	QMutexLocker locker(global_mutex());
	Q_ASSERT(global);
	if(!global)
		return;

	global->app_name = s;
}

QString arrayToHex(const QSecureArray &a)
{
	return Hex().arrayToString(a);
}

QByteArray hexToArray(const QString &str)
{
	return Hex().stringToArray(str).toByteArray();
}

static Provider *getProviderForType(const QString &type, const QString &provider)
{
	Provider *p = 0;
	bool scanned = false;
	if(!provider.isEmpty())
	{
		// try using specific provider
		p = global->manager.findFor(provider, type);
		if(!p)
		{
			// maybe this provider is new, so scan and try again
			global->scan();
			scanned = true;
			p = global->manager.findFor(provider, type);
		}
	}
	if(!p)
	{
		// try using some other provider
		p = global->manager.findFor(QString(), type);
		if((!p || p->name() == "default") && !scanned)
		{
			// maybe there are new providers, so scan and try again
			//   before giving up or using default
			global->scan();
			scanned = true;
			p = global->manager.findFor(QString(), type);
		}
	}

	return p;
}

Provider::Context *doCreateContext(Provider *p, const QString &type)
{
	// load config at the first context create.  this is mainly
	//   to work around locking issues present during actual
	//   plugin loading
	QVariantMap conf = getProviderConfig(p->name());
	if(!conf.isEmpty())
		p->configChanged(conf);

	return p->createContext(type);
}

Provider::Context *getContext(const QString &type, const QString &provider)
{
	Provider *p;
	{
		QMutexLocker locker(global_mutex());
		Q_ASSERT(global);
		if(!global)
			return 0;

		p = getProviderForType(type, provider);
		if(!p)
			return 0;
	}

	return doCreateContext(p, type);
}

Provider::Context *getContext(const QString &type, Provider *_p)
{
	Provider *p;
	{
		QMutexLocker locker(global_mutex());
		Q_ASSERT(global);
		if(!global)
			return 0;

		p = global->manager.find(_p);
		if(!p)
			return 0;
	}

	return doCreateContext(p, type);
}

//----------------------------------------------------------------------------
// Initializer
//----------------------------------------------------------------------------
Initializer::Initializer(MemoryMode m, int prealloc)
{
	init(m, prealloc);
}

Initializer::~Initializer()
{
	deinit();
}

//----------------------------------------------------------------------------
// Provider
//----------------------------------------------------------------------------
Provider::~Provider()
{
}

void Provider::init()
{
}

QString Provider::credit() const
{
	return QString();
}

QVariantMap Provider::defaultConfig() const
{
	return QVariantMap();
}

void Provider::configChanged(const QVariantMap &)
{
}

Provider::Context::Context(Provider *parent, const QString &type)
:QObject()
{
	_provider = parent;
	_type = type;
}

Provider::Context::Context(const Context &from)
:QObject()
{
	_provider = from._provider;
	_type = from._type;
}

Provider::Context::~Context()
{
}

Provider *Provider::Context::provider() const
{
	return _provider;
}

QString Provider::Context::type() const
{
	return _type;
}

bool Provider::Context::sameProvider(const Context *c) const
{
	return (c->provider() == _provider);
}

//----------------------------------------------------------------------------
// BasicContext
//----------------------------------------------------------------------------
BasicContext::BasicContext(Provider *parent, const QString &type)
:Context(parent, type)
{
	moveToThread(0); // no thread association
}

BasicContext::BasicContext(const BasicContext &from)
:Context(from)
{
	moveToThread(0); // no thread association
}

BasicContext::~BasicContext()
{
}

//----------------------------------------------------------------------------
// PKeyBase
//----------------------------------------------------------------------------
PKeyBase::PKeyBase(Provider *p, const QString &type)
:BasicContext(p, type)
{
}

int PKeyBase::maximumEncryptSize(EncryptionAlgorithm) const
{
	return 0;
}

QSecureArray PKeyBase::encrypt(const QSecureArray &, EncryptionAlgorithm)
{
	return QSecureArray();
}

bool PKeyBase::decrypt(const QSecureArray &, QSecureArray *, EncryptionAlgorithm)
{
	return false;
}

void PKeyBase::startSign(SignatureAlgorithm, SignatureFormat)
{
}

void PKeyBase::startVerify(SignatureAlgorithm, SignatureFormat)
{
}

void PKeyBase::update(const QSecureArray &)
{
}

QSecureArray PKeyBase::endSign()
{
	return QSecureArray();
}

bool PKeyBase::endVerify(const QSecureArray &)
{
	return false;
}

SymmetricKey PKeyBase::deriveKey(const PKeyBase &)
{
	return SymmetricKey();
}

//----------------------------------------------------------------------------
// PKeyContext
//----------------------------------------------------------------------------
QSecureArray PKeyContext::publicToDER() const
{
	return QSecureArray();
}

QString PKeyContext::publicToPEM() const
{
	return QString();
}

ConvertResult PKeyContext::publicFromDER(const QSecureArray &)
{
	return ErrorDecode;
}

ConvertResult PKeyContext::publicFromPEM(const QString &)
{
	return ErrorDecode;
}

QSecureArray PKeyContext::privateToDER(const QSecureArray &, PBEAlgorithm) const
{
	return QSecureArray();
}

QString PKeyContext::privateToPEM(const QSecureArray &, PBEAlgorithm) const
{
	return QString();
}

ConvertResult PKeyContext::privateFromDER(const QSecureArray &, const QSecureArray &)
{
	return ErrorDecode;
}

ConvertResult PKeyContext::privateFromPEM(const QString &, const QSecureArray &)
{
	return ErrorDecode;
}

//----------------------------------------------------------------------------
// KeyStoreEntryContext
//----------------------------------------------------------------------------
KeyBundle KeyStoreEntryContext::keyBundle() const
{
	return KeyBundle();
}

Certificate KeyStoreEntryContext::certificate() const
{
	return Certificate();
}

CRL KeyStoreEntryContext::crl() const
{
	return CRL();
}

PGPKey KeyStoreEntryContext::pgpSecretKey() const
{
	return PGPKey();
}

PGPKey KeyStoreEntryContext::pgpPublicKey() const
{
	return PGPKey();
}

bool KeyStoreEntryContext::ensureAccess()
{
	return true;
}

//----------------------------------------------------------------------------
// KeyStoreListContext
//----------------------------------------------------------------------------
void KeyStoreListContext::start()
{
}

void KeyStoreListContext::setUpdatesEnabled(bool enabled)
{
	if(enabled)
		QMetaObject::invokeMethod(this, "busyEnd", Qt::QueuedConnection);
}

bool KeyStoreListContext::isReadOnly(int) const
{
	return true;
}

KeyStoreEntryContext *KeyStoreListContext::entry(int id, const QString &entryId)
{
	KeyStoreEntryContext *out = 0;
	QList<KeyStoreEntryContext*> list = entryList(id);
	for(int n = 0; n < list.count(); ++n)
	{
		if(list[n]->id() == entryId)
		{
			out = list.takeAt(n);
			break;
		}
	}
	qDeleteAll(list);
	return out;
}

KeyStoreEntryContext *KeyStoreListContext::entryPassive(const QString &storeId, const QString &entryId)
{
	Q_UNUSED(storeId);
	Q_UNUSED(entryId);
	return 0;
}

bool KeyStoreListContext::writeEntry(int, const KeyBundle &)
{
	return false;
}

bool KeyStoreListContext::writeEntry(int, const Certificate &)
{
	return false;
}

bool KeyStoreListContext::writeEntry(int, const CRL &)
{
	return false;
}

PGPKey KeyStoreListContext::writeEntry(int, const PGPKey &)
{
	return PGPKey();
}

bool KeyStoreListContext::removeEntry(int, const QString &)
{
	return false;
}

void KeyStoreListContext::submitPassphrase(int, int, const QSecureArray &)
{
}

void KeyStoreListContext::rejectPassphraseRequest(int, int)
{
}

//----------------------------------------------------------------------------
// TLSContext
//----------------------------------------------------------------------------
void TLSContext::setMTU(int)
{
}

//----------------------------------------------------------------------------
// SMSContext
//----------------------------------------------------------------------------
void SMSContext::setTrustedCertificates(const CertificateCollection &)
{
}

void SMSContext::setPrivateKeys(const QList<SecureMessageKey> &)
{
}

//----------------------------------------------------------------------------
// BufferedComputation
//----------------------------------------------------------------------------
BufferedComputation::~BufferedComputation()
{
}

QSecureArray BufferedComputation::process(const QSecureArray &a)
{
	clear();
	update(a);
	return final();
}

//----------------------------------------------------------------------------
// Filter
//----------------------------------------------------------------------------
Filter::~Filter()
{
}

QSecureArray Filter::process(const QSecureArray &a)
{
	clear();
	QSecureArray buf = update(a);
	if(!ok())
		return QSecureArray();
	QSecureArray fin = final();
	if(!ok())
		return QSecureArray();
	int oldsize = buf.size();
	buf.resize(oldsize + fin.size());
	memcpy(buf.data() + oldsize, fin.data(), fin.size());
	return buf;
}

//----------------------------------------------------------------------------
// Algorithm
//----------------------------------------------------------------------------
class Algorithm::Private : public QSharedData
{
public:
	Provider::Context *c;

	Private(Provider::Context *context)
	{
		c = context;
		//printf("** [%p] Algorithm Created\n", c);
	}

	Private(const Private &from) : QSharedData(from)
	{
		c = from.c->clone();
		//printf("** [%p] Algorithm Copied (to [%p])\n", from.c, c);
	}

	~Private()
	{
		//printf("** [%p] Algorithm Destroyed\n", c);
		delete c;
	}
};

Algorithm::Algorithm()
{
}

Algorithm::Algorithm(const QString &type, const QString &provider)
{
	change(type, provider);
}

Algorithm::Algorithm(const Algorithm &from)
{
	*this = from;
}

Algorithm::~Algorithm()
{
}

Algorithm & Algorithm::operator=(const Algorithm &from)
{
	d = from.d;
	return *this;
}

QString Algorithm::type() const
{
	if(d)
		return d->c->type();
	else
		return QString();
}

Provider *Algorithm::provider() const
{
	if(d)
		return d->c->provider();
	else
		return 0;
}

Provider::Context *Algorithm::context()
{
	if(d)
		return d->c;
	else
		return 0;
}

const Provider::Context *Algorithm::context() const
{
	if(d)
		return d->c;
	else
		return 0;
}

void Algorithm::change(Provider::Context *c)
{
	if(c)
		d = new Private(c);
	else
		d = 0;
}

void Algorithm::change(const QString &type, const QString &provider)
{
	if(!type.isEmpty())
		change(getContext(type, provider));
	else
		change(0);
}

Provider::Context *Algorithm::takeContext()
{
	if(d)
	{
		Provider::Context *c = d->c; // should cause a detach
		d->c = 0;
		d = 0;
		return c;
	}
	else
		return 0;
}

//----------------------------------------------------------------------------
// SymmetricKey
//----------------------------------------------------------------------------
SymmetricKey::SymmetricKey()
{
}

SymmetricKey::SymmetricKey(int size)
{
	set(globalRNG().nextBytes(size));
}

SymmetricKey::SymmetricKey(const QSecureArray &a)
{
	set(a);
}

SymmetricKey::SymmetricKey(const QByteArray &a)
{
	set(QSecureArray(a));
}

/* from libgcrypt-1.2.0 */
static unsigned char desWeakKeyTable[64][8] =
{
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /*w*/
	{ 0x00, 0x00, 0x1e, 0x1e, 0x00, 0x00, 0x0e, 0x0e },
	{ 0x00, 0x00, 0xe0, 0xe0, 0x00, 0x00, 0xf0, 0xf0 },
	{ 0x00, 0x00, 0xfe, 0xfe, 0x00, 0x00, 0xfe, 0xfe },
	{ 0x00, 0x1e, 0x00, 0x1e, 0x00, 0x0e, 0x00, 0x0e }, /*sw*/
	{ 0x00, 0x1e, 0x1e, 0x00, 0x00, 0x0e, 0x0e, 0x00 },
	{ 0x00, 0x1e, 0xe0, 0xfe, 0x00, 0x0e, 0xf0, 0xfe },
	{ 0x00, 0x1e, 0xfe, 0xe0, 0x00, 0x0e, 0xfe, 0xf0 },
	{ 0x00, 0xe0, 0x00, 0xe0, 0x00, 0xf0, 0x00, 0xf0 }, /*sw*/
	{ 0x00, 0xe0, 0x1e, 0xfe, 0x00, 0xf0, 0x0e, 0xfe },
	{ 0x00, 0xe0, 0xe0, 0x00, 0x00, 0xf0, 0xf0, 0x00 },
	{ 0x00, 0xe0, 0xfe, 0x1e, 0x00, 0xf0, 0xfe, 0x0e },
	{ 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe }, /*sw*/
	{ 0x00, 0xfe, 0x1e, 0xe0, 0x00, 0xfe, 0x0e, 0xf0 },
	{ 0x00, 0xfe, 0xe0, 0x1e, 0x00, 0xfe, 0xf0, 0x0e },
	{ 0x00, 0xfe, 0xfe, 0x00, 0x00, 0xfe, 0xfe, 0x00 },
	{ 0x1e, 0x00, 0x00, 0x1e, 0x0e, 0x00, 0x00, 0x0e },
	{ 0x1e, 0x00, 0x1e, 0x00, 0x0e, 0x00, 0x0e, 0x00 }, /*sw*/
	{ 0x1e, 0x00, 0xe0, 0xfe, 0x0e, 0x00, 0xf0, 0xfe },
	{ 0x1e, 0x00, 0xfe, 0xe0, 0x0e, 0x00, 0xfe, 0xf0 },
	{ 0x1e, 0x1e, 0x00, 0x00, 0x0e, 0x0e, 0x00, 0x00 },
	{ 0x1e, 0x1e, 0x1e, 0x1e, 0x0e, 0x0e, 0x0e, 0x0e }, /*w*/
	{ 0x1e, 0x1e, 0xe0, 0xe0, 0x0e, 0x0e, 0xf0, 0xf0 },
	{ 0x1e, 0x1e, 0xfe, 0xfe, 0x0e, 0x0e, 0xfe, 0xfe },
	{ 0x1e, 0xe0, 0x00, 0xfe, 0x0e, 0xf0, 0x00, 0xfe },
	{ 0x1e, 0xe0, 0x1e, 0xe0, 0x0e, 0xf0, 0x0e, 0xf0 }, /*sw*/
	{ 0x1e, 0xe0, 0xe0, 0x1e, 0x0e, 0xf0, 0xf0, 0x0e },
	{ 0x1e, 0xe0, 0xfe, 0x00, 0x0e, 0xf0, 0xfe, 0x00 },
	{ 0x1e, 0xfe, 0x00, 0xe0, 0x0e, 0xfe, 0x00, 0xf0 },
	{ 0x1e, 0xfe, 0x1e, 0xfe, 0x0e, 0xfe, 0x0e, 0xfe }, /*sw*/
	{ 0x1e, 0xfe, 0xe0, 0x00, 0x0e, 0xfe, 0xf0, 0x00 },
	{ 0x1e, 0xfe, 0xfe, 0x1e, 0x0e, 0xfe, 0xfe, 0x0e },
	{ 0xe0, 0x00, 0x00, 0xe0, 0xf0, 0x00, 0x00, 0xf0 },
	{ 0xe0, 0x00, 0x1e, 0xfe, 0xf0, 0x00, 0x0e, 0xfe },
	{ 0xe0, 0x00, 0xe0, 0x00, 0xf0, 0x00, 0xf0, 0x00 }, /*sw*/
	{ 0xe0, 0x00, 0xfe, 0x1e, 0xf0, 0x00, 0xfe, 0x0e },
	{ 0xe0, 0x1e, 0x00, 0xfe, 0xf0, 0x0e, 0x00, 0xfe },
	{ 0xe0, 0x1e, 0x1e, 0xe0, 0xf0, 0x0e, 0x0e, 0xf0 },
	{ 0xe0, 0x1e, 0xe0, 0x1e, 0xf0, 0x0e, 0xf0, 0x0e }, /*sw*/
	{ 0xe0, 0x1e, 0xfe, 0x00, 0xf0, 0x0e, 0xfe, 0x00 },
	{ 0xe0, 0xe0, 0x00, 0x00, 0xf0, 0xf0, 0x00, 0x00 },
	{ 0xe0, 0xe0, 0x1e, 0x1e, 0xf0, 0xf0, 0x0e, 0x0e },
	{ 0xe0, 0xe0, 0xe0, 0xe0, 0xf0, 0xf0, 0xf0, 0xf0 }, /*w*/
	{ 0xe0, 0xe0, 0xfe, 0xfe, 0xf0, 0xf0, 0xfe, 0xfe },
	{ 0xe0, 0xfe, 0x00, 0x1e, 0xf0, 0xfe, 0x00, 0x0e },
	{ 0xe0, 0xfe, 0x1e, 0x00, 0xf0, 0xfe, 0x0e, 0x00 },
	{ 0xe0, 0xfe, 0xe0, 0xfe, 0xf0, 0xfe, 0xf0, 0xfe }, /*sw*/
	{ 0xe0, 0xfe, 0xfe, 0xe0, 0xf0, 0xfe, 0xfe, 0xf0 },
	{ 0xfe, 0x00, 0x00, 0xfe, 0xfe, 0x00, 0x00, 0xfe },
	{ 0xfe, 0x00, 0x1e, 0xe0, 0xfe, 0x00, 0x0e, 0xf0 },
	{ 0xfe, 0x00, 0xe0, 0x1e, 0xfe, 0x00, 0xf0, 0x0e },
	{ 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00 }, /*sw*/
	{ 0xfe, 0x1e, 0x00, 0xe0, 0xfe, 0x0e, 0x00, 0xf0 },
	{ 0xfe, 0x1e, 0x1e, 0xfe, 0xfe, 0x0e, 0x0e, 0xfe },
	{ 0xfe, 0x1e, 0xe0, 0x00, 0xfe, 0x0e, 0xf0, 0x00 },
	{ 0xfe, 0x1e, 0xfe, 0x1e, 0xfe, 0x0e, 0xfe, 0x0e }, /*sw*/
	{ 0xfe, 0xe0, 0x00, 0x1e, 0xfe, 0xf0, 0x00, 0x0e },
	{ 0xfe, 0xe0, 0x1e, 0x00, 0xfe, 0xf0, 0x0e, 0x00 },
	{ 0xfe, 0xe0, 0xe0, 0xfe, 0xfe, 0xf0, 0xf0, 0xfe },
	{ 0xfe, 0xe0, 0xfe, 0xe0, 0xfe, 0xf0, 0xfe, 0xf0 }, /*sw*/
	{ 0xfe, 0xfe, 0x00, 0x00, 0xfe, 0xfe, 0x00, 0x00 },
	{ 0xfe, 0xfe, 0x1e, 0x1e, 0xfe, 0xfe, 0x0e, 0x0e },
	{ 0xfe, 0xfe, 0xe0, 0xe0, 0xfe, 0xfe, 0xf0, 0xf0 },
	{ 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe }  /*w*/
};

bool SymmetricKey::isWeakDESKey()
{
	if(size() != 8)
		return false; // dubious
	QSecureArray workingCopy(8);
	// clear parity bits
	for(uint i = 0; i < 8; i++)
		workingCopy[i] = (data()[i]) & 0xfe;

	for(int n = 0; n < 64; n++)
	{
		if(memcmp(workingCopy.data(), desWeakKeyTable[n], 8) == 0)
			return true;
	}
	return false;
}

//----------------------------------------------------------------------------
// InitializationVector
//----------------------------------------------------------------------------
InitializationVector::InitializationVector()
{
}

InitializationVector::InitializationVector(int size)
{
	set(globalRNG().nextBytes(size));
}

InitializationVector::InitializationVector(const QSecureArray &a)
{
	set(a);
}

InitializationVector::InitializationVector(const QByteArray &a)
{
	set(QSecureArray(a));
}

//----------------------------------------------------------------------------
// Event
//----------------------------------------------------------------------------
class Event::Private : public QSharedData
{
public:
	Type type;
	Source source;
	PasswordStyle style;
	QString ks, kse;
	QString fname;
	void *ptr;
};

Event::Event()
{
}

Event::Event(const Event &from)
:d(from.d)
{
}

Event::~Event()
{
}

Event & Event::operator=(const Event &from)
{
	d = from.d;
	return *this;
}

bool Event::isNull() const
{
	return (d ? false : true);
}

Event::Type Event::type() const
{
	return d->type;
}

Event::Source Event::source() const
{
	return d->source;
}

Event::PasswordStyle Event::passwordStyle() const
{
	return d->style;
}

QString Event::keyStoreId() const
{
	return d->ks;
}

QString Event::keyStoreEntryId() const
{
	return d->kse;
}

QString Event::fileName() const
{
	return d->fname;
}

void *Event::ptr() const
{
	return d->ptr;
}

void Event::setPasswordKeyStore(PasswordStyle pstyle, const QString &keyStoreId, const QString &keyStoreEntryId, void *ptr)
{
	if(!d)
		d = new Private;
	d->type = Password;
	d->source = KeyStore;
	d->style = pstyle;
	d->ks = keyStoreId;
	d->kse = keyStoreEntryId;
	d->fname = QString();
	d->ptr = ptr;
}

void Event::setPasswordData(PasswordStyle pstyle, const QString &fileName, void *ptr)
{
	if(!d)
		d = new Private;
	d->type = Password;
	d->source = Data;
	d->style = pstyle;
	d->ks = QString();
	d->kse = QString();
	d->fname = fileName;
	d->ptr = ptr;
}

void Event::setToken(const QString &keyStoreEntryId, void *ptr)
{
	if(!d)
		d = new Private;
	d->type = Token;
	d->source = KeyStore;
	d->style = StylePassword;
	d->ks = QString();
	d->kse = keyStoreEntryId;
	d->fname = QString();
	d->ptr = ptr;
}

//----------------------------------------------------------------------------
// EventHandler
//----------------------------------------------------------------------------
class AskerItem : public QObject
{
	Q_OBJECT
public:
	enum Type { Password, Token };

	Type type;
	PasswordAsker *passwordAsker;
	TokenAsker *tokenAsker;

	bool accepted;
	QSecureArray password;
	int id;
	bool waiting;
	bool done;
	QTimer readyTrigger;

	QMutex m;
	QWaitCondition w;

	QThread *emitFrom;
	QList<EventHandler*> handlers;

	AskerItem(QObject *parent = 0);
	~AskerItem();

	void ask(const Event &e);
	void handlerGone(EventHandler *h);
	void unreg();
	void waitForFinished();
	void finish();

	// handler calls this
	void accept_password(const QSecureArray &_password);
	void accept_token();
	void reject();

private slots:
	void ready_timeout();
};

Q_GLOBAL_STATIC(QMutex, g_event_mutex)

class EventGlobal;
static EventGlobal *g_event = 0;

class EventGlobal
{
public:
	QList<EventHandler*> handlers;
	int next_id;

	EventGlobal()
	{
		next_id = 0;
	}

	static void ensureInit()
	{
		if(g_event)
			return;

		g_event = new EventGlobal;
	}

	static void tryCleanup()
	{
		if(!g_event)
			return;

		if(g_event->handlers.isEmpty() /*&& g_event->askers.isEmpty()*/)
		{
			delete g_event;
			g_event = 0;
		}
	}
};

class EventHandlerPrivate
{
public:
	bool started;
	QHash<int,AskerItem*> askers;
	QMutex m;

	EventHandlerPrivate()
	{
		started = false;
	}
};

EventHandler::EventHandler(QObject *parent)
:QObject(parent)
{
	d = new EventHandlerPrivate;
}

EventHandler::~EventHandler()
{
	//MX QMutexLocker locker(&d->m);

	if(d->started)
	{
		for(int n = 0; n < d->askers.count(); ++n)
		{
			d->askers[n]->handlerGone(this);
		}

		/*if(d->askers.isEmpty())
		{
			// if this flag is set, then AskerItem::ask() will do
			//   the global cleanup instead of us.

			//g_event->deleteLaterList += this;
		}
		else
		{*/
			QMutexLocker locker(g_event_mutex());
			g_event->handlers.removeAll(this);
			EventGlobal::tryCleanup();
		//}
	}

	delete d;
}

void EventHandler::start()
{
	QMutexLocker locker(g_event_mutex());
	EventGlobal::ensureInit();
	d->started = true;
	g_event->handlers += this;
}

void EventHandler::submitPassword(int id, const QSecureArray &password)
{
	//MX QMutexLocker locker(&d->m);
	AskerItem *ai = d->askers.value(id);
	if(!ai || ai->type != AskerItem::Password)
		return;

	ai->accept_password(password);
}

void EventHandler::tokenOkay(int id)
{
	//MX QMutexLocker locker(&d->m);
	AskerItem *ai = d->askers.value(id);
	if(!ai || ai->type != AskerItem::Token)
		return;

	ai->accept_token();
}

void EventHandler::reject(int id)
{
	//MX QMutexLocker locker(&d->m);
	AskerItem *ai = d->askers.value(id);
	if(!ai)
		return;

	ai->reject();
}

//----------------------------------------------------------------------------
// PasswordAsker
//----------------------------------------------------------------------------
class PasswordAskerPrivate
{
public:
	AskerItem *ai;

	PasswordAskerPrivate()
	{
		ai = 0;
	}

	~PasswordAskerPrivate()
	{
		delete ai;
	}
};

PasswordAsker::PasswordAsker(QObject *parent)
:QObject(parent)
{
	d = new PasswordAskerPrivate;
}

PasswordAsker::~PasswordAsker()
{
	delete d;
}

void PasswordAsker::ask(Event::PasswordStyle pstyle, const QString &keyStoreId, const QString &keyStoreEntryId, void *ptr)
{
	Event e;
	e.setPasswordKeyStore(pstyle, keyStoreId, keyStoreEntryId, ptr);
	delete d->ai;
	d->ai = new AskerItem(this);
	d->ai->type = AskerItem::Password;
	d->ai->passwordAsker = this;
	d->ai->ask(e);
}

void PasswordAsker::ask(Event::PasswordStyle pstyle, const QString &fileName, void *ptr)
{
	Event e;
	e.setPasswordData(pstyle, fileName, ptr);
	delete d->ai;
	d->ai = new AskerItem(this);
	d->ai->type = AskerItem::Password;
	d->ai->passwordAsker = this;
	d->ai->ask(e);
}

void PasswordAsker::cancel()
{
	delete d->ai;
	d->ai = 0;
}

void PasswordAsker::waitForResponse()
{
	d->ai->waitForFinished();
}

bool PasswordAsker::accepted() const
{
	return d->ai->accepted;
}

QSecureArray PasswordAsker::password() const
{
	return d->ai->password;
}

//----------------------------------------------------------------------------
// TokenAsker
//----------------------------------------------------------------------------
class TokenAskerPrivate
{
public:
	AskerItem *ai;

	TokenAskerPrivate()
	{
		ai = 0;
	}

	~TokenAskerPrivate()
	{
		delete ai;
	}
};

TokenAsker::TokenAsker(QObject *parent)
:QObject(parent)
{
	d = new TokenAskerPrivate;
}

TokenAsker::~TokenAsker()
{
	delete d;
}

void TokenAsker::ask(const QString &keyStoreEntryId, void *ptr)
{
	Event e;
	e.setToken(keyStoreEntryId, ptr);
	delete d->ai;
	d->ai = new AskerItem(this);
	d->ai->type = AskerItem::Token;
	d->ai->tokenAsker = this;
	d->ai->ask(e);
}

void TokenAsker::cancel()
{
	delete d->ai;
	d->ai = 0;
}

void TokenAsker::waitForResponse()
{
	d->ai->waitForFinished();
}

bool TokenAsker::accepted() const
{
	return d->ai->accepted;
}

//----------------------------------------------------------------------------
// AskerItem
//----------------------------------------------------------------------------
AskerItem::AskerItem(QObject *parent)
:QObject(parent), readyTrigger(this)
{
	readyTrigger.setSingleShot(true);
	connect(&readyTrigger, SIGNAL(timeout()), SLOT(ready_timeout()));
	done = true;
	emitFrom = 0;
}

AskerItem::~AskerItem()
{
	if(!done)
		unreg();
}

void AskerItem::ask(const Event &e)
{
	accepted = false;
	password = QSecureArray();
	waiting = false;
	done = false;

	{
		//MX QMutexLocker locker(g_event_mutex());

		// no handlers?  reject the request then
		if(!g_event || g_event->handlers.isEmpty())
		{
			done = true;
			readyTrigger.start();
			return;
		}

		id = g_event->next_id++;
		handlers = g_event->handlers;

		//g_event->askers.insert(id, this);
	}

	/*{
		QMutexLocker locker(g_event_mutex());
		++g_event->emitrefs;
	}*/

	{
		//MX QMutexLocker locker(&m);
		for(int n = 0; n < handlers.count(); ++n)
		{
			//MX QMutexLocker locker(&handlers[n]->d->m);
			handlers[n]->d->askers.insert(id, this);
		}

		emitFrom = QThread::currentThread();

		for(int n = 0; n < handlers.count(); ++n)
		{
			EventHandler *h = handlers[n];
			if(!h)
				continue;

			emit h->eventReady(id, e);

			/*{
				QMutexLocker locker(g_event->deleteLaterMutex);
				if(g_event->deleteLaterList.contains(h))
				{
					g_event->deleteLaterList.removeAll(this);
					g_event->handlers.removeAll(this);
					--n; // adjust iterator
				}
				else
					h->d->sync_emit = false;
			}*/

			if(done)
				break;
		}

		emitFrom = 0;
	}

	/*{
		QMutexLocker locker(g_event_mutex());
		--g_event->emitrefs;
		if(g_event->emitrefs == 0)
		{*/
			// clean up null handlers
			for(int n = 0; n < handlers.count(); ++n)
			{
				if(handlers[n] == 0)
				{
					handlers.removeAt(n);
					--n; // adjust position
				}
			}
		/*}
	}*/

	if(done)
	{
		//MX QMutexLocker locker(&m);
		unreg();
		readyTrigger.start();
		return;
	}
}

void AskerItem::handlerGone(EventHandler *h)
{
	if(QThread::currentThread() == emitFrom)
	{
		int n = handlers.indexOf(h);
		if(n != -1)
			handlers[n] = 0;
	}
	else
	{
		//MX QMutexLocker locker(&m);
		handlers.removeAll(h);
	}
}

void AskerItem::unreg()
{
	//if(!done)
	//{
		//QMutexLocker locker(&m);
		for(int n = 0; n < handlers.count(); ++n)
		{
			//MX QMutexLocker locker(&handlers[n]->d->m);
			handlers[n]->d->askers.remove(id);
		}
	//}
}

void AskerItem::waitForFinished()
{
	//MX QMutexLocker locker(&m);

	if(done)
	{
		readyTrigger.stop();
		return;
	}

	waiting = true;
	w.wait(&m);

	unreg();
}

void AskerItem::finish()
{
	done = true;
	if(waiting)
		w.wakeOne();
	else
	{
		if(type == Password)
			emit passwordAsker->responseReady();
		else // Token
			emit tokenAsker->responseReady();
	}
}

void AskerItem::accept_password(const QSecureArray &_password)
{
	//MX QMutexLocker locker(&m);
	accepted = true;
	password = _password;
	finish();
}

void AskerItem::accept_token()
{
	//MX QMutexLocker locker(&m);
	accepted = true;
	finish();
}

void AskerItem::reject()
{
	//MX QMutexLocker locker(&m);
	finish();
}

void AskerItem::ready_timeout()
{
	if(type == Password)
		emit passwordAsker->responseReady();
	else // Token
		emit tokenAsker->responseReady();
}

}

#include "qca_core.moc"
