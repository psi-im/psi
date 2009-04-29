/*
 * Copyright (C) 2003-2008  Justin Karneges <justin@affinix.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include "qca_core.h"

#include "qca_plugin.h"
#include "qca_textfilter.h"
#include "qca_cert.h"
#include "qca_keystore.h"
#include "qcaprovider.h"

// for qAddPostRoutine
#include <QCoreApplication>

#include <QMutex>
#include <QSettings>
#include <QVariantMap>
#include <QWaitCondition>

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
	bool loaded;
	bool first_scan;
	QString app_name;
	QMutex name_mutex;
	ProviderManager *manager;
	QMutex scan_mutex;
	Random *rng;
	QMutex rng_mutex;
	Logger *logger;
	QVariantMap properties;
	QMutex prop_mutex;
	QMap<QString,QVariantMap> config;
	QMutex config_mutex;
	QMutex logger_mutex;

	Global()
	{
		refs = 0;
		secmem = false;
		loaded = false;
		first_scan = false;
		rng = 0;
		logger = 0;
		manager = new ProviderManager;
	}

	~Global()
	{
		KeyStoreManager::shutdown();
		delete rng;
		rng = 0;
		delete manager;
		manager = 0;
		delete logger;
		logger = 0;
	}

	void ensure_loaded()
	{
		// probably we shouldn't overload scan mutex, or else rename it
		QMutexLocker locker(&scan_mutex);
		if(!loaded)
		{
			loaded = true;
			manager->setDefault(create_default_provider()); // manager owns it
		}
	}

	bool ensure_first_scan()
	{
		scan_mutex.lock();
		if(!first_scan)
		{
			first_scan = true;
			manager->scan();
			scan_mutex.unlock();
			return true;
		}
		scan_mutex.unlock();
		return false;
	}

	void scan()
	{
		scan_mutex.lock();
		first_scan = true;
		manager->scan();
		scan_mutex.unlock();
	}

	void ksm_scan()
	{
		KeyStoreManager::scan();
	}

	Logger *get_logger()
	{
		QMutexLocker locker(&logger_mutex);
		if(!logger)
		{
			logger = new Logger;

			// needed so deinit may delete the logger regardless
			//   of what thread the logger was created from
			logger->moveToThread(0);
		}
		return logger;
	}

	void unloadAllPlugins()
	{
		KeyStoreManager::shutdown();

		// if the global_rng was owned by a plugin, then delete it
		rng_mutex.lock();
		if(rng && (rng->provider() != manager->find("default")))
		{
			delete rng;
			rng = 0;
		}
		rng_mutex.unlock();

		manager->unloadAll();
	}
};

Q_GLOBAL_STATIC(QMutex, global_mutex)
static Global *global = 0;

static bool features_have(const QStringList &have, const QStringList &want)
{
	foreach(const QString &i, want)
	{
		if(!have.contains(i))
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

static bool global_check()
{
	Q_ASSERT(global);
	if(!global)
		return false;
	return true;
}

static bool global_check_load()
{
	Q_ASSERT(global);
	if(!global)
		return false;
	global->ensure_loaded();
	return true;
}

QMutex *global_random_mutex()
{
	return &global->rng_mutex;
}

Random *global_random()
{
	if(!global->rng)
		global->rng = new Random;
	return global->rng;
}

bool haveSecureMemory()
{
	if(!global_check())
		return false;

	return global->secmem;
}

bool haveSecureRandom()
{
	if(!global_check_load())
		return false;

	QMutexLocker locker(global_random_mutex());
	if(global_random()->provider()->name() != "default")
		return true;

	return false;
}

bool isSupported(const QStringList &features, const QString &provider)
{
	if(!global_check_load())
		return false;

	// single
	if(!provider.isEmpty())
	{
		Provider *p = global->manager->find(provider);
		if(!p)
		{
			// ok, try scanning for new stuff
			global->scan();
			p = global->manager->find(provider);
		}

		if(p && features_have(p->features(), features))
			return true;
	}
	// all
	else
	{
		if(features_have(global->manager->allFeatures(), features))
			return true;

		global->manager->appendDiagnosticText(QString("Scanning to find features: %1\n").arg(features.join(" ")));

		// ok, try scanning for new stuff
		global->scan();

		if(features_have(global->manager->allFeatures(), features))
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
	if(!global_check_load())
		return QStringList();

	// query all features
	global->scan();
	return global->manager->allFeatures();
}

QStringList defaultFeatures()
{
	if(!global_check_load())
		return QStringList();

	return global->manager->find("default")->features();
}

ProviderList providers()
{
	if(!global_check_load())
		return ProviderList();

	global->ensure_first_scan();

	return global->manager->providers();
}

bool insertProvider(Provider *p, int priority)
{
	if(!global_check_load())
		return false;

	global->ensure_first_scan();

	return global->manager->add(p, priority);
}

void setProviderPriority(const QString &name, int priority)
{
	if(!global_check_load())
		return;

	global->ensure_first_scan();

	global->manager->changePriority(name, priority);
}

int providerPriority(const QString &name)
{
	if(!global_check_load())
		return -1;

	global->ensure_first_scan();

	return global->manager->getPriority(name);
}

Provider *findProvider(const QString &name)
{
	if(!global_check_load())
		return 0;

	global->ensure_first_scan();

	return global->manager->find(name);
}

Provider *defaultProvider()
{
	if(!global_check_load())
		return 0;

	return global->manager->find("default");
}

void scanForPlugins()
{
	if(!global_check_load())
		return;

	global->scan();
	global->ksm_scan();
}

void unloadAllPlugins()
{
	if(!global_check_load())
		return;

	global->unloadAllPlugins();
}

QString pluginDiagnosticText()
{
	if(!global_check_load())
		return QString();

	return global->manager->diagnosticText();
}

void clearPluginDiagnosticText()
{
	if(!global_check_load())
		return;

	global->manager->clearDiagnosticText();
}

void appendPluginDiagnosticText(const QString &text)
{
	if(!global_check_load())
		return;

	global->manager->appendDiagnosticText(text);
}

void setProperty(const QString &name, const QVariant &value)
{
	if(!global_check_load())
		return;

	QMutexLocker locker(&global->prop_mutex);

	global->properties[name] = value;
}

QVariant getProperty(const QString &name)
{
	if(!global_check_load())
		return QVariant();

	QMutexLocker locker(&global->prop_mutex);

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
	QSettings settings("Affinix", "QCA2");
	settings.beginGroup("ProviderConfig");
	QStringList providerNames = settings.value("providerNames").toStringList();
	if(!providerNames.contains(name))
		return QVariantMap();

	settings.beginGroup(name);
	QStringList keys = settings.childKeys();
	QVariantMap map;
	foreach(const QString &key, keys)
		map[key] = settings.value(key);
	settings.endGroup();

	if(!configIsValid(map))
		return QVariantMap();
	return map;
}

static bool writeConfig(const QString &name, const QVariantMap &config, bool systemWide = false)
{
	QSettings settings(QSettings::NativeFormat, systemWide ? QSettings::SystemScope : QSettings::UserScope, "Affinix", "QCA2");
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
	if(!global_check_load())
		return;

	if(!configIsValid(config))
		return;

	global->config_mutex.lock();
	global->config[name] = config;
	global->config_mutex.unlock();

	Provider *p = findProvider(name);
	if(p)
		p->configChanged(config);
}

QVariantMap getProviderConfig(const QString &name)
{
	if(!global_check_load())
		return QVariantMap();

	QVariantMap conf;

	global->config_mutex.lock();

	// try loading from persistent storage
	conf = readConfig(name);

	// if not, load the one from memory
	if(conf.isEmpty())
		conf = global->config.value(name);

	global->config_mutex.unlock();

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
	if(!global_check_load())
		return;

	QMutexLocker locker(&global->config_mutex);

	QVariantMap conf = global->config.value(name);
	if(conf.isEmpty())
		return;

	writeConfig(name, conf);
}

QVariantMap getProviderConfig_internal(Provider *p)
{
	QVariantMap conf;
	QString name = p->name();

	global->config_mutex.lock();

	// try loading from persistent storage
	conf = readConfig(name);

	// if not, load the one from memory
	if(conf.isEmpty())
		conf = global->config.value(name);

	global->config_mutex.unlock();

	// if provider doesn't exist or doesn't have a valid config form,
	//   use the config we loaded
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

QString globalRandomProvider()
{
	QMutexLocker locker(global_random_mutex());
	return global_random()->provider()->name();
}

void setGlobalRandomProvider(const QString &provider)
{
	QMutexLocker locker(global_random_mutex());
	delete global->rng;
	global->rng = new Random(provider);
}

Logger *logger()
{
	return global->get_logger();
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
	if(!global_check())
		return QString();

	QMutexLocker locker(&global->name_mutex);

	return global->app_name;
}

void setAppName(const QString &s)
{
	if(!global_check())
		return;

	QMutexLocker locker(&global->name_mutex);

	global->app_name = s;
}

QString arrayToHex(const QByteArray &a)
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
	bool scanned = global->ensure_first_scan();
	if(!provider.isEmpty())
	{
		// try using specific provider
		p = global->manager->findFor(provider, type);
		if(!p && !scanned)
		{
			// maybe this provider is new, so scan and try again
			global->scan();
			scanned = true;
			p = global->manager->findFor(provider, type);
		}
	}
	if(!p)
	{
		// try using some other provider
		p = global->manager->findFor(QString(), type);

		// note: we used to rescan if no provider was found or if
		//   the only found provider was 'default'.  now we only
		//   rescan if no provider was found.  this optimizes lookups
		//   for features that are in the default provider (such as
		//   'sha1') when no other plugin is available.  the drawback
		//   is that if a plugin is installed later during runtime,
		//   then it won't be picked up without restarting the
		//   application or manually calling QCA::scanForPlugins.
		//if((!p || p->name() == "default") && !scanned)
		if(!p && !scanned)
		{
			// maybe there are new providers, so scan and try again
			//   before giving up or using default
			global->scan();
			scanned = true;
			p = global->manager->findFor(QString(), type);
		}
	}

	return p;
}

static inline Provider::Context *doCreateContext(Provider *p, const QString &type)
{
	return p->createContext(type);
}

Provider::Context *getContext(const QString &type, const QString &provider)
{
	if(!global_check_load())
		return 0;

	Provider *p;
	{
		p = getProviderForType(type, provider);
		if(!p)
			return 0;
	}

	return doCreateContext(p, type);
}

Provider::Context *getContext(const QString &type, Provider *_p)
{
	if(!global_check_load())
		return 0;

	Provider *p;
	{
		p = global->manager->find(_p);
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

void Provider::deinit()
{
}

int Provider::version() const
{
	return 0;
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
// InfoContext
//----------------------------------------------------------------------------
QStringList InfoContext::supportedHashTypes() const
{
	return QStringList();
}

QStringList InfoContext::supportedCipherTypes() const
{
	return QStringList();
}

QStringList InfoContext::supportedMACTypes() const
{
	return QStringList();
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

SecureArray PKeyBase::encrypt(const SecureArray &, EncryptionAlgorithm)
{
	return SecureArray();
}

bool PKeyBase::decrypt(const SecureArray &, SecureArray *, EncryptionAlgorithm)
{
	return false;
}

void PKeyBase::startSign(SignatureAlgorithm, SignatureFormat)
{
}

void PKeyBase::startVerify(SignatureAlgorithm, SignatureFormat)
{
}

void PKeyBase::update(const MemoryRegion &)
{
}

QByteArray PKeyBase::endSign()
{
	return QByteArray();
}

bool PKeyBase::endVerify(const QByteArray &)
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
QByteArray PKeyContext::publicToDER() const
{
	return QByteArray();
}

QString PKeyContext::publicToPEM() const
{
	return QString();
}

ConvertResult PKeyContext::publicFromDER(const QByteArray &)
{
	return ErrorDecode;
}

ConvertResult PKeyContext::publicFromPEM(const QString &)
{
	return ErrorDecode;
}

SecureArray PKeyContext::privateToDER(const SecureArray &, PBEAlgorithm) const
{
	return SecureArray();
}

QString PKeyContext::privateToPEM(const SecureArray &, PBEAlgorithm) const
{
	return QString();
}

ConvertResult PKeyContext::privateFromDER(const SecureArray &, const SecureArray &)
{
	return ErrorDecode;
}

ConvertResult PKeyContext::privateFromPEM(const QString &, const SecureArray &)
{
	return ErrorDecode;
}

//----------------------------------------------------------------------------
// KeyStoreEntryContext
//----------------------------------------------------------------------------
bool KeyStoreEntryContext::isAvailable() const
{
	return true;
}

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
	QMetaObject::invokeMethod(this, "busyEnd", Qt::QueuedConnection);
}

void KeyStoreListContext::setUpdatesEnabled(bool)
{
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

KeyStoreEntryContext *KeyStoreListContext::entryPassive(const QString &serialized)
{
	Q_UNUSED(serialized);
	return 0;
}

QString KeyStoreListContext::writeEntry(int, const KeyBundle &)
{
	return QString();
}

QString KeyStoreListContext::writeEntry(int, const Certificate &)
{
	return QString();
}

QString KeyStoreListContext::writeEntry(int, const CRL &)
{
	return QString();
}

QString KeyStoreListContext::writeEntry(int, const PGPKey &)
{
	return QString();
}

bool KeyStoreListContext::removeEntry(int, const QString &)
{
	return false;
}

//----------------------------------------------------------------------------
// TLSContext
//----------------------------------------------------------------------------
void TLSContext::setMTU(int)
{
}

//----------------------------------------------------------------------------
// MessageContext
//----------------------------------------------------------------------------
QString MessageContext::diagnosticText() const
{
	return QString();
}

//----------------------------------------------------------------------------
// SMSContext
//----------------------------------------------------------------------------
void SMSContext::setTrustedCertificates(const CertificateCollection &)
{
}

void SMSContext::setUntrustedCertificates(const CertificateCollection &)
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

MemoryRegion BufferedComputation::process(const MemoryRegion &a)
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

MemoryRegion Filter::process(const MemoryRegion &a)
{
	clear();
	MemoryRegion buf = update(a);
	if(!ok())
		return MemoryRegion();
	MemoryRegion fin = final();
	if(!ok())
		return MemoryRegion();
	if(buf.isSecure() || fin.isSecure())
		return (SecureArray(buf) + SecureArray(fin));
	else
		return (buf.toByteArray() + fin.toByteArray());
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
	set(Random::randomArray(size));
}

SymmetricKey::SymmetricKey(const SecureArray &a)
{
	set(a);
}

SymmetricKey::SymmetricKey(const QByteArray &a)
{
	set(SecureArray(a));
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
	SecureArray workingCopy(8);
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
	set(Random::randomArray(size));
}

InitializationVector::InitializationVector(const SecureArray &a)
{
	set(a);
}

InitializationVector::InitializationVector(const QByteArray &a)
{
	set(SecureArray(a));
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
	KeyStoreInfo ksi;
	KeyStoreEntry kse;
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

KeyStoreInfo Event::keyStoreInfo() const
{
	return d->ksi;
}

KeyStoreEntry Event::keyStoreEntry() const
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

void Event::setPasswordKeyStore(PasswordStyle pstyle, const KeyStoreInfo &keyStoreInfo, const KeyStoreEntry &keyStoreEntry, void *ptr)
{
	if(!d)
		d = new Private;
	d->type = Password;
	d->source = KeyStore;
	d->style = pstyle;
	d->ksi = keyStoreInfo;
	d->kse = keyStoreEntry;
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
	d->ksi = KeyStoreInfo();
	d->kse = KeyStoreEntry();
	d->fname = fileName;
	d->ptr = ptr;
}

void Event::setToken(const KeyStoreInfo &keyStoreInfo, const KeyStoreEntry &keyStoreEntry, void *ptr)
{
	if(!d)
		d = new Private;
	d->type = Token;
	d->source = KeyStore;
	d->style = StylePassword;
	d->ksi = keyStoreInfo;
	d->kse = keyStoreEntry;
	d->fname = QString();
	d->ptr = ptr;
}

//----------------------------------------------------------------------------
// EventGlobal
//----------------------------------------------------------------------------
class HandlerBase : public QObject
{
	Q_OBJECT
public:
	HandlerBase(QObject *parent = 0) : QObject(parent)
	{
	}

protected slots:
	virtual void ask(int id, const QCA::Event &e) = 0;
};

class AskerBase : public QObject
{
	Q_OBJECT
public:
	AskerBase(QObject *parent = 0) : QObject(parent)
	{
	}

	virtual void set_accepted(const SecureArray &password) = 0;
	virtual void set_rejected() = 0;
};

static void handler_add(HandlerBase *h, int pos = -1);
static void handler_remove(HandlerBase *h);
static void handler_accept(HandlerBase *h, int id, const SecureArray &password);
static void handler_reject(HandlerBase *h, int id);
static bool asker_ask(AskerBase *a, const Event &e);
static void asker_cancel(AskerBase *a);

Q_GLOBAL_STATIC(QMutex, g_event_mutex)

class EventGlobal;
static EventGlobal *g_event = 0;

class EventGlobal
{
public:
	class HandlerItem
	{
	public:
		HandlerBase *h;
		QList<int> ids;
	};

	class AskerItem
	{
	public:
		AskerBase *a;
		int id;
		Event event;
		int handler_pos;
	};

	QList<HandlerItem> handlers;
	QList<AskerItem> askers;

	int next_id;

	EventGlobal()
	{
		qRegisterMetaType<Event>("QCA::Event");
		qRegisterMetaType<SecureArray>("QCA::SecureArray");
		next_id = 0;
	}

	int findHandlerItem(HandlerBase *h)
	{
		for(int n = 0; n < handlers.count(); ++n)
		{
			if(handlers[n].h == h)
				return n;
		}
		return -1;
	}

	int findAskerItem(AskerBase *a)
	{
		for(int n = 0; n < askers.count(); ++n)
		{
			if(askers[n].a == a)
				return n;
		}
		return -1;
	}

	int findAskerItemById(int id)
	{
		for(int n = 0; n < askers.count(); ++n)
		{
			if(askers[n].id == id)
				return n;
		}
		return -1;
	}

	void ask(int asker_at)
	{
		AskerItem &i = askers[asker_at];

		g_event->handlers[i.handler_pos].ids += i.id;
		QMetaObject::invokeMethod(handlers[i.handler_pos].h, "ask",
			Qt::QueuedConnection, Q_ARG(int, i.id),
			Q_ARG(QCA::Event, i.event));
	}

	void reject(int asker_at)
	{
		AskerItem &i = askers[asker_at];

		// look for the next usable handler
		int pos = -1;
		for(int n = i.handler_pos + 1; n < g_event->handlers.count(); ++n)
		{
			// handler and asker can't be in the same thread
			//Q_ASSERT(g_event->handlers[n].h->thread() != i.a->thread());
			//if(g_event->handlers[n].h->thread() != i.a->thread())
			//{
				pos = n;
				break;
			//}
		}

		// if there is one, try it
		if(pos != -1)
		{
			i.handler_pos = pos;
			ask(asker_at);
		}
		// if not, send official reject
		else
		{
			AskerBase *asker = i.a;
			askers.removeAt(asker_at);

			asker->set_rejected();
		}
	}
};

void handler_add(HandlerBase *h, int pos)
{
	QMutexLocker locker(g_event_mutex());
	if(!g_event)
		g_event = new EventGlobal;

	EventGlobal::HandlerItem i;
	i.h = h;

	if(pos != -1)
	{
		g_event->handlers.insert(pos, i);

		// adjust handler positions
		for(int n = 0; n < g_event->askers.count(); ++n)
		{
			if(g_event->askers[n].handler_pos >= pos)
				g_event->askers[n].handler_pos++;
		}
	}
	else
		g_event->handlers += i;
}

void handler_remove(HandlerBase *h)
{
	QMutexLocker locker(g_event_mutex());
	Q_ASSERT(g_event);
	if(!g_event)
		return;
	int at = g_event->findHandlerItem(h);
	Q_ASSERT(at != -1);
	if(at == -1)
		return;

	QList<int> ids = g_event->handlers[at].ids;
	g_event->handlers.removeAt(at);

	// adjust handler positions within askers
	for(int n = 0; n < g_event->askers.count(); ++n)
	{
		if(g_event->askers[n].handler_pos >= at)
			g_event->askers[n].handler_pos--;
	}

	// reject all askers
	foreach(int id, ids)
	{
		int asker_at = g_event->findAskerItemById(id);
		Q_ASSERT(asker_at != -1);

		g_event->reject(asker_at);
	}

	if(g_event->handlers.isEmpty())
	{
		delete g_event;
		g_event = 0;
	}
}

void handler_accept(HandlerBase *h, int id, const SecureArray &password)
{
	QMutexLocker locker(g_event_mutex());
	Q_ASSERT(g_event);
	if(!g_event)
		return;
	int at = g_event->findHandlerItem(h);
	Q_ASSERT(at != -1);
	if(at == -1)
		return;
	int asker_at = g_event->findAskerItemById(id);
	Q_ASSERT(asker_at != -1);
	if(asker_at == -1)
		return;

	g_event->handlers[at].ids.removeAll(g_event->askers[asker_at].id);

	AskerBase *asker = g_event->askers[asker_at].a;
	asker->set_accepted(password);
}

void handler_reject(HandlerBase *h, int id)
{
	QMutexLocker locker(g_event_mutex());
	Q_ASSERT(g_event);
	if(!g_event)
		return;
	int at = g_event->findHandlerItem(h);
	Q_ASSERT(at != -1);
	if(at == -1)
		return;
	int asker_at = g_event->findAskerItemById(id);
	Q_ASSERT(asker_at != -1);
	if(asker_at == -1)
		return;

	g_event->handlers[at].ids.removeAll(g_event->askers[asker_at].id);

	g_event->reject(asker_at);
}

bool asker_ask(AskerBase *a, const Event &e)
{
	QMutexLocker locker(g_event_mutex());
	if(!g_event)
		return false;

	int pos = -1;
	for(int n = 0; n < g_event->handlers.count(); ++n)
	{
		// handler and asker can't be in the same thread
		//Q_ASSERT(g_event->handlers[n].h->thread() != a->thread());
		//if(g_event->handlers[n].h->thread() != a->thread())
		//{
			pos = n;
			break;
		//}
	}
	if(pos == -1)
		return false;

	EventGlobal::AskerItem i;
	i.a = a;
	i.id = g_event->next_id++;
	i.event = e;
	i.handler_pos = pos;
	g_event->askers += i;
	int asker_at = g_event->askers.count() - 1;

	g_event->ask(asker_at);
	return true;
}

void asker_cancel(AskerBase *a)
{
	QMutexLocker locker(g_event_mutex());
	if(!g_event)
		return;
	int at = g_event->findAskerItem(a);
	if(at == -1)
		return;

	for(int n = 0; n < g_event->handlers.count(); ++n)
		g_event->handlers[n].ids.removeAll(g_event->askers[at].id);

	g_event->askers.removeAt(at);
}

//----------------------------------------------------------------------------
// EventHandler
//----------------------------------------------------------------------------
class EventHandler::Private : public HandlerBase
{
	Q_OBJECT
public:
	EventHandler *q;
	bool started;
	QList<int> activeIds;

	Private(EventHandler *_q) : HandlerBase(_q), q(_q)
	{
		started = false;
	}

public slots:
	virtual void ask(int id, const QCA::Event &e)
	{
		activeIds += id;
		emit q->eventReady(id, e);
	}
};

EventHandler::EventHandler(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

EventHandler::~EventHandler()
{
	if(d->started)
	{
		foreach(int id, d->activeIds)
			handler_reject(d, id);

		handler_remove(d);
	}

	delete d;
}

void EventHandler::start()
{
	d->started = true;
	handler_add(d);
}

void EventHandler::submitPassword(int id, const SecureArray &password)
{
	if(!d->activeIds.contains(id))
		return;

	d->activeIds.removeAll(id);
	handler_accept(d, id, password);
}

void EventHandler::tokenOkay(int id)
{
	if(!d->activeIds.contains(id))
		return;

	d->activeIds.removeAll(id);
	handler_accept(d, id, SecureArray());
}

void EventHandler::reject(int id)
{
	if(!d->activeIds.contains(id))
		return;

	d->activeIds.removeAll(id);
	handler_reject(d, id);
}

//----------------------------------------------------------------------------
// PasswordAsker
//----------------------------------------------------------------------------
class AskerPrivate : public AskerBase
{
	Q_OBJECT
public:
	enum Type { Password, Token };

	Type type;
	PasswordAsker *passwordAsker;
	TokenAsker *tokenAsker;

	QMutex m;
	QWaitCondition w;

	bool accepted;
	SecureArray password;
	bool waiting;
	bool done;

	AskerPrivate(PasswordAsker *parent) : AskerBase(parent)
	{
		passwordAsker = parent;
		tokenAsker = 0;
		type = Password;
		accepted = false;
		waiting = false;
		done = true;
	}

	AskerPrivate(TokenAsker *parent) : AskerBase(parent)
	{
		passwordAsker = 0;
		tokenAsker = parent;
		type = Token;
		accepted = false;
		waiting = false;
		done = true;
	}

	void ask(const Event &e)
	{
		accepted = false;
		waiting = false;
		done = false;
		password.clear();

		if(!asker_ask(this, e))
		{
			done = true;
			QMetaObject::invokeMethod(this, "emitResponseReady", Qt::QueuedConnection);
		}
	}

	void cancel()
	{
		if(!done)
			asker_cancel(this);
	}

	virtual void set_accepted(const SecureArray &_password)
	{
		QMutexLocker locker(&m);
		accepted = true;
		password = _password;
		done = true;
		if(waiting)
			w.wakeOne();
		else
			QMetaObject::invokeMethod(this, "emitResponseReady", Qt::QueuedConnection);
	}

	virtual void set_rejected()
	{
		QMutexLocker locker(&m);
		done = true;
		if(waiting)
			w.wakeOne();
		else
			QMetaObject::invokeMethod(this, "emitResponseReady", Qt::QueuedConnection);
	}

	void waitForResponse()
	{
		QMutexLocker locker(&m);
		if(done)
			return;
		waiting = true;
		w.wait(&m);
		waiting = false;
	}

public slots:
	virtual void emitResponseReady() = 0;
};

class PasswordAsker::Private : public AskerPrivate
{
public:
	Private(PasswordAsker *_q) : AskerPrivate(_q)
	{
	}

	virtual void emitResponseReady()
	{
		emit passwordAsker->responseReady();
	}
};

PasswordAsker::PasswordAsker(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

PasswordAsker::~PasswordAsker()
{
	delete d;
}

void PasswordAsker::ask(Event::PasswordStyle pstyle, const KeyStoreInfo &keyStoreInfo, const KeyStoreEntry &keyStoreEntry, void *ptr)
{
	Event e;
	e.setPasswordKeyStore(pstyle, keyStoreInfo, keyStoreEntry, ptr);
	d->ask(e);
}

void PasswordAsker::ask(Event::PasswordStyle pstyle, const QString &fileName, void *ptr)
{
	Event e;
	e.setPasswordData(pstyle, fileName, ptr);
	d->ask(e);
}

void PasswordAsker::cancel()
{
	d->cancel();
}

void PasswordAsker::waitForResponse()
{
	d->waitForResponse();
}

bool PasswordAsker::accepted() const
{
	return d->accepted;
}

SecureArray PasswordAsker::password() const
{
	return d->password;
}

//----------------------------------------------------------------------------
// TokenAsker
//----------------------------------------------------------------------------
class TokenAsker::Private : public AskerPrivate
{
public:
	Private(TokenAsker *_q) : AskerPrivate(_q)
	{
	}

	virtual void emitResponseReady()
	{
		emit tokenAsker->responseReady();
	}
};

TokenAsker::TokenAsker(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

TokenAsker::~TokenAsker()
{
	delete d;
}

void TokenAsker::ask(const KeyStoreInfo &keyStoreInfo, const KeyStoreEntry &keyStoreEntry, void *ptr)
{
	Event e;
	e.setToken(keyStoreInfo, keyStoreEntry, ptr);
	d->ask(e);
}

void TokenAsker::cancel()
{
	d->cancel();
}

void TokenAsker::waitForResponse()
{
	d->waitForResponse();
}

bool TokenAsker::accepted() const
{
	return d->accepted;
}

}

#include "qca_core.moc"
