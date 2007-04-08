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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "qca_keystore.h"

#include "qcaprovider.h"

namespace QCA {

Provider::Context *getContext(const QString &type, Provider *p);

static QMutex *keyStoreMutex = 0;
static int tracker_register(KeyStore *ks, const QString &str); // returns trackerId
static void tracker_unregister(KeyStore *ks, int trackerId);
static KeyStoreListContext *tracker_get_owner(int trackerId);
static int tracker_get_storeContextId(int trackerId);
static QString tracker_get_storeId(int trackerId);

//----------------------------------------------------------------------------
// KeyStoreEntry
//----------------------------------------------------------------------------
KeyStoreEntry::KeyStoreEntry()
{
}

KeyStoreEntry::KeyStoreEntry(const KeyStoreEntry &from)
:Algorithm(from)
{
}

KeyStoreEntry::~KeyStoreEntry()
{
}

KeyStoreEntry & KeyStoreEntry::operator=(const KeyStoreEntry &from)
{
	Algorithm::operator=(from);
	return *this;
}

bool KeyStoreEntry::isNull() const
{
	return (!context() ? true : false);
}

KeyStoreEntry::Type KeyStoreEntry::type() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->type();
}

QString KeyStoreEntry::name() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->name();
}

QString KeyStoreEntry::id() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->id();
}

KeyBundle KeyStoreEntry::keyBundle() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->keyBundle();
}

Certificate KeyStoreEntry::certificate() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->certificate();
}

CRL KeyStoreEntry::crl() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->crl();
}

PGPKey KeyStoreEntry::pgpSecretKey() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->pgpSecretKey();
}

PGPKey KeyStoreEntry::pgpPublicKey() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->pgpPublicKey();
}

//----------------------------------------------------------------------------
// KeyStore
//----------------------------------------------------------------------------
class KeyStore::Private
{
public:
	Private() : trackerId(-1), ksl(0), contextId(-1)
	{
	}

	void invalidate()
	{
		trackerId = -1;
		ksl = 0;
		contextId = -1;
		id = QString();
	}

	int trackerId;
	KeyStoreListContext *ksl;
	int contextId; // store context id
	QString id; // public store id
};

KeyStore::KeyStore(const QString &id, QObject *parent)
:QObject(parent)
{
	QMutexLocker locker(keyStoreMutex);
	d = new Private;
	d->trackerId = tracker_register(this, id);
	if(d->trackerId != -1)
	{
		d->ksl = tracker_get_owner(d->trackerId);
		d->contextId = tracker_get_storeContextId(d->trackerId);
		d->id = tracker_get_storeId(d->trackerId);
	}
}

KeyStore::~KeyStore()
{
	QMutexLocker locker(keyStoreMutex);
	if(d->trackerId != -1)
		tracker_unregister(this, d->trackerId);
	delete d;
}

bool KeyStore::isValid() const
{
	QMutexLocker locker(keyStoreMutex);
	return (d->trackerId != -1);
}

KeyStore::Type KeyStore::type() const
{
	QMutexLocker locker(keyStoreMutex);
	if(d->trackerId == -1)
		return KeyStore::System;
	return d->ksl->type(d->contextId);
}

QString KeyStore::name() const
{
	// FIXME: QMutexLocker locker(keyStoreMutex);
	if(d->trackerId == -1)
		return QString();
	return d->ksl->name(d->contextId);
}

QString KeyStore::id() const
{
	QMutexLocker locker(keyStoreMutex);
	if(d->trackerId == -1)
		return QString();
	return d->id;
}

bool KeyStore::isReadOnly() const
{
	QMutexLocker locker(keyStoreMutex);
	if(d->trackerId == -1)
		return false;
	return d->ksl->isReadOnly(d->contextId);
}

QList<KeyStoreEntry> KeyStore::entryList() const
{
	QMutexLocker locker(keyStoreMutex);
	QList<KeyStoreEntry> out;
	if(d->trackerId == -1)
		return out;
	QList<KeyStoreEntryContext*> list = d->ksl->entryList(d->contextId);
	for(int n = 0; n < list.count(); ++n)
	{
		KeyStoreEntry entry;
		entry.change(list[n]);
		out.append(entry);
	}
	//printf("KeyStore::entryList(): %d entries\n", out.count());
	return out;
}

bool KeyStore::holdsTrustedCertificates() const
{
	QMutexLocker locker(keyStoreMutex);
	if(d->trackerId == -1)
		return false;
	QList<KeyStoreEntry::Type> list = d->ksl->entryTypes(d->contextId);
	if(list.contains(KeyStoreEntry::TypeCertificate) || list.contains(KeyStoreEntry::TypeCRL))
		return true;
	return false;
}

bool KeyStore::holdsIdentities() const
{
	QMutexLocker locker(keyStoreMutex);
	if(d->trackerId == -1)
		return false;
	QList<KeyStoreEntry::Type> list = d->ksl->entryTypes(d->contextId);
	if(list.contains(KeyStoreEntry::TypeKeyBundle) || list.contains(KeyStoreEntry::TypePGPSecretKey))
		return true;
	return false;
}

bool KeyStore::holdsPGPPublicKeys() const
{
	QMutexLocker locker(keyStoreMutex);
	if(d->trackerId == -1)
		return false;
	QList<KeyStoreEntry::Type> list = d->ksl->entryTypes(d->contextId);
	if(list.contains(KeyStoreEntry::TypePGPPublicKey))
		return true;
	return false;
}

bool KeyStore::writeEntry(const KeyBundle &kb)
{
	// TODO
	Q_UNUSED(kb);
	return false;
}

bool KeyStore::writeEntry(const Certificate &cert)
{
	// TODO
	Q_UNUSED(cert);
	return false;
}

bool KeyStore::writeEntry(const CRL &crl)
{
	// TODO
	Q_UNUSED(crl);
	return false;
}

PGPKey KeyStore::writeEntry(const PGPKey &key)
{
	// TODO
	Q_UNUSED(key);
	return PGPKey();
}

bool KeyStore::removeEntry(const QString &id)
{
	// TODO
	Q_UNUSED(id);
	return false;
}

/*void KeyStore::submitPassphrase(int requestId, const QSecureArray &passphrase)
{
	// FIXME: QMutexLocker locker(keyStoreMutex);
	if(d->trackerId == -1)
		return;
	d->ksl->submitPassphrase(d->contextId, requestId, passphrase);
}

void KeyStore::rejectPassphraseRequest(int requestId)
{
	// FIXME: QMutexLocker locker(keyStoreMutex);
	if(d->trackerId == -1)
		return;
	d->ksl->rejectPassphraseRequest(d->contextId, requestId);
}*/

void KeyStore::invalidate()
{
	d->invalidate();
}

//----------------------------------------------------------------------------
// KeyStoreTracker
//----------------------------------------------------------------------------

/*
  How this stuff works:

  KeyStoreListContext is queried for a list of KeyStoreContexts.  A signal
  is used to indicate when the list may have changed, so polling for changes
  is not necessary.  Every context object created internally by the provider
  will have a unique contextId, and this is used for detecting changes.  Even
  if a user removes and inserts the same smart card device, which has the
  same deviceId, the contextId will ALWAYS be different.  If a previously
  known contextId is missing from a later queried list, then it means the
  associated KeyStoreContext has been deleted by the provider (the manager
  here does not delete them, it just throws away any references).  It is
  recommended that the provider just use a counter for the contextId,
  incrementing the value anytime a new context is made.
*/

/*
- all keystorelistcontexts in a side thread
- all keystores also exist in the same side thread
- mutex protect the entire system, so that nothing happens simultaneously:
  - every keystore / keystoreentry access method
  - provider scanning
  - keystore removal
- call keystoremanager.start() to initiate system
- waitfordiscovery counts for all providers found on first scan

- maybe keystore should thread-safe protect all calls

scenarios to handle:
  - ksm.start shouldn't block
  - keystore in available list, but gone by the time it is requested
  - keystore is unavailable during a call to a keystoreentry method
  - keystore/keystoreentry methods called simultaneously from different threads
  - and of course, objects from keystores should work, despite being created
    in the keystore thread
*/

class KeyStoreManagerPrivate
{
public:
	KeyStoreManager *q;
	KeyStoreThread *thread;
	QString dtext;

	KeyStoreManagerPrivate(KeyStoreManager *);
	~KeyStoreManagerPrivate();
};

class KeyStoreTracker;
static KeyStoreTracker *trackerInstance = 0;

class KeyStoreTracker : public QObject
{
	Q_OBJECT
public:
	// "public"
	class Item
	{
	public:
		int trackerId;
		KeyStoreListContext *owner;
		int storeContextId;
		QString storeId;

		Item() : trackerId(-1), owner(0), storeContextId(-1) {}
	};
	QList<Item*> stores;
	QHash<int, Item*> storesByTrackerId;
	QHash<QString, Item*> storesByStoreId;

	typedef QList<KeyStore*> PublicReferences;
	QHash<int, PublicReferences> refHash;

	// "private"
	KeyStoreManager *ksm;
	int trackerId_at;
	QSet<Provider*> providerSet;
	bool first_scan;
	QList<KeyStoreListContext*> sources;
	QSet<KeyStoreListContext*> initialDiscoverySet;
	bool has_initdisco;

	QList<Item> added;
	QList<int> removed;

	KeyStoreTracker(QObject *parent = 0) : QObject(parent)
	{
		trackerInstance = this;
		trackerId_at = 0;
		first_scan = true;
		ksm = keyStoreManager(); // global qca function
		keyStoreMutex = new QMutex;
		has_initdisco = false;
	}

	~KeyStoreTracker()
	{
		qDeleteAll(stores);
		qDeleteAll(sources);
		delete keyStoreMutex;
		keyStoreMutex = 0;
		trackerInstance = 0;
	}

	bool hasInitialDiscovery() const
	{
		QMutexLocker locker(keyStoreMutex);
		return has_initdisco;
	}

public slots:
	void scan()
	{
		//QTimer::singleShot(3000, this, SIGNAL(initialDiscovery()));
		//QTimer::singleShot(6000, this, SIGNAL(initialDiscovery()));

		QMutexLocker locker(keyStoreMutex);

		// grab providers (and default)
		ProviderList list = providers();
		list.append(defaultProvider());

		for(int n = 0; n < list.count(); ++n)
		{
			Provider *p = list[n];
			if(p->features().contains("keystorelist") && !providerSet.contains(p))
			{
				KeyStoreListContext *c = static_cast<KeyStoreListContext *>(getContext("keystorelist", p));
				if(!c)
					continue;
				providerSet += p;
				sources += c;
				if(first_scan)
				{
					initialDiscoverySet += c;
					connect(c, SIGNAL(busyEnd()), SLOT(ksl_busyEnd()));
				}
				connect(c, SIGNAL(updated()), SLOT(ksl_updated()));
				connect(c, SIGNAL(diagnosticText(const QString &)), SLOT(ksl_diagnosticText(const QString &)));
				connect(c, SIGNAL(storeUpdated(int)), SLOT(ksl_storeUpdated(int)));
				//connect(c, SIGNAL(storeNeedPassphrase(int, int, const QString &)), SLOT(ksl_storeNeedPassphrase(int, int, const QString &)), Qt::DirectConnection);
				c->start();
				c->setUpdatesEnabled(true);
				check(c);
			}
		}

		flush();

		// nothing found?
		if(first_scan && initialDiscoverySet.isEmpty())
			emit initialDiscovery();

		first_scan = false;
	}

signals:
	void initialDiscovery();

private slots:
	void ksl_busyEnd()
	{
		QMutexLocker locker(keyStoreMutex);

		//printf("ksl_initialDiscovery [%p]\n", sender);

		KeyStoreListContext *ksl = (KeyStoreListContext *)sender();
		check(ksl);
		flush();

		if(!initialDiscoverySet.contains(ksl))
			return;

		//keyStoreMutex->lock();
		initialDiscoverySet.remove(ksl);
		if(initialDiscoverySet.isEmpty())
		{
		//	keyStoreMutex->unlock();
			has_initdisco = true;
			emit initialDiscovery();
		}
		//else
		//	keyStoreMutex->unlock();
	}

	void ksl_updated()
	{
		QMutexLocker locker(keyStoreMutex);
		KeyStoreListContext *ksl = (KeyStoreListContext *)sender();

		check(ksl);
		flush();
	}

	void ksl_diagnosticText(const QString &str)
	{
		KeyStoreListContext *ksl = (KeyStoreListContext *)sender();
		QString name = ksl->provider()->name();
		ksm->d->dtext += QString("%1: %2\n").arg(name).arg(str);
	}

	void ksl_storeUpdated(int contextId)
	{
		QMutexLocker locker(keyStoreMutex);
		KeyStoreListContext *ksl = (KeyStoreListContext *)sender();

		Item *item = findByOwnerAndContext(ksl, contextId);
		if(!item)
			return;

		PublicReferences refs = refHash.value(item->trackerId);
		for(int n = 0; n < refs.count(); ++n)
		{
			KeyStore *ks = refs[n];
			emit ks->updated();
		}
	}

	/*void ksl_storeNeedPassphrase(int contextId, int requestId, const QString &entryId)
	{
		KeyStoreListContext *ksl = (KeyStoreListContext *)sender();

		PublicReferences refs;

		{
			// FIXME: QMutexLocker locker(keyStoreMutex);

			//printf("ksl_storeNeedPassphrase [%p]\n", sender);

			Item *item = findByOwnerAndContext(ksl, contextId);
			if(!item)
				return;

			refs = refHash.value(item->trackerId);
		}

		for(int n = 0; n < refs.count(); ++n)
		{
			KeyStore *ks = refs[n];
			emit ks->needPassphrase(requestId, entryId); // FIXME
		}
	}*/

private:
	Item *findByOwnerAndContext(KeyStoreListContext *owner, int contextId)
	{
		Item *item = 0;
		for(int n = 0; n < stores.count(); ++n)
		{
			if(stores[n]->owner == owner && stores[n]->storeContextId == contextId)
			{
				item = stores[n];
				break;
			}
		}
		return item;
	}

	bool have_store_local(KeyStoreListContext *c, int id)
	{
		for(int n = 0; n < stores.count(); ++n)
		{
			if(stores[n]->owner == c && stores[n]->storeContextId == id)
				return true;
		}
		return false;
	}

	bool have_store_incoming(const QList<int> &keyStores, int id)
	{
		for(int n = 0; n < keyStores.count(); ++n)
		{
			if(keyStores[n] == id)
				return true;
		}
		return false;
	}

	void check(KeyStoreListContext *c)
	{
		QList<int> keyStores = c->keyStores();
		for(int n = 0; n < keyStores.count(); ++n)
		{
			if(!have_store_local(c, keyStores[n]))
			{
				Item i;
				i.owner = c;
				i.storeContextId = keyStores[n];
				added += i;
			}
		}
		for(int n = 0; n < stores.count(); ++n)
		{
			if(stores[n]->owner == c && !have_store_incoming(keyStores, stores[n]->storeContextId))
				removed += stores[n]->trackerId;
		}
	}

	void flush()
	{
		// removed
		for(int n = 0; n < removed.count(); ++n)
		{
			int trackerId = removed[n];
			int index = -1;
			for(int i = 0; i < stores.count(); ++i)
			{
				if(stores[i]->trackerId == trackerId)
				{
					index = i;
					break;
				}
			}
			if(index != -1)
			{
				Item *item = stores[index];
				stores.removeAt(index);
				storesByTrackerId.remove(item->trackerId);
				storesByStoreId.remove(item->storeId);
				delete item;
				PublicReferences refs = refHash.value(trackerId);
				refHash.remove(trackerId);
				for(int refIdx = 0; refIdx < refs.count(); ++refIdx)
				{
					KeyStore *ks = refs[refIdx];
					ks->invalidate();
					emit ks->unavailable();
				}
			}
		}
		removed.clear();

		// added
		for(int n = 0; n < added.count(); ++n)
		{
			Item *i = new Item(added[n]);
			i->storeId = i->owner->storeId(i->storeContextId);
			if(i->storeId.isEmpty())
			{
				delete i;
				continue;
			}
			i->trackerId = trackerId_at++;
			stores += i;
			storesByTrackerId[i->trackerId] = i;
			storesByStoreId[i->storeId] = i;
			emit ksm->keyStoreAvailable(i->storeId);
		}
		added.clear();
	}
};

int tracker_register(KeyStore *ks, const QString &str)
{
	KeyStoreTracker::Item *item = trackerInstance->storesByStoreId.value(str);
	if(!item)
		return -1;
	int trackerId = item->trackerId;
	trackerInstance->refHash[trackerId] += ks;
	return trackerId;
}

void tracker_unregister(KeyStore *ks, int trackerId)
{
	trackerInstance->refHash[trackerId].removeAll(ks);
}

KeyStoreListContext *tracker_get_owner(int trackerId)
{
	KeyStoreTracker::Item *item = trackerInstance->storesByTrackerId.value(trackerId);
	if(!item)
		return 0;
	return item->owner;
}

int tracker_get_storeContextId(int trackerId)
{
	KeyStoreTracker::Item *item = trackerInstance->storesByTrackerId.value(trackerId);
	if(!item)
		return -1;
	return item->storeContextId;
}

QString tracker_get_storeId(int trackerId)
{
	KeyStoreTracker::Item *item = trackerInstance->storesByTrackerId.value(trackerId);
	if(!item)
		return QString();
	return item->storeId;
}

/*class KeyStoreApp : public QObject
{
	Q_OBJECT
public:
	KeyStoreApp()
	{
	}

	~KeyStoreApp()
	{
	}

signals:
	void foo();

public slots:
	void quit()
	{
	}
};*/

class KeyStoreThread : public QThread
{
	Q_OBJECT
public:
	QMutex control_mutex;
	QWaitCondition control_wait;
	QEventLoop *loop;
	bool waiting;

	KeyStoreThread(QObject *parent) : QThread(parent)
	{
		loop = 0;
		waiting = false;
	}

	~KeyStoreThread()
	{
		stop();
	}

	void start()
	{
		control_mutex.lock();
		QThread::start();
		control_wait.wait(&control_mutex);
		control_mutex.unlock();
		//printf("thread: started\n");
	}

	void stop()
	{
		//printf("thread: stopping\n");
		{
			QMutexLocker locker(&control_mutex);
			if(loop)
				QMetaObject::invokeMethod(loop, "quit");
		}
		wait();
		//printf("thread: stopped\n");
	}

	void scan()
	{
		//printf("scan\n");
		QMetaObject::invokeMethod(trackerInstance, "scan", Qt::QueuedConnection);
	}

	void waitForInitialDiscovery()
	{
		if(trackerInstance->hasInitialDiscovery())
			return;

		//printf("thread: waiting for initial discovery\n");
		control_mutex.lock();
		waiting = true;
		control_wait.wait(&control_mutex);
		waiting = false;
		control_mutex.unlock();
		//printf("thread: wait finished\n");
	}

protected:
	virtual void run()
	{
		control_mutex.lock();
		KeyStoreTracker tracker;
		connect(&tracker, SIGNAL(initialDiscovery()), SLOT(tracker_initialDiscovery()), Qt::DirectConnection);
		//KeyStoreApp app;
		//connect(&app, SIGNAL(foo()), SLOT(tracker_initialDiscovery()), Qt::DirectConnection);
		//QTimer::singleShot(3000, &app, SIGNAL(foo()));
		//QTimer::singleShot(6000, &app, SIGNAL(foo()));
		loop = new QEventLoop; // TODO: delete this at some point
		control_wait.wakeOne();
		control_mutex.unlock();
		loop->exec();
		QMutexLocker locker(&control_mutex);
		loop = 0;
	}

private slots:
	void tracker_initialDiscovery()
	{
		//printf("tracker_initialDiscovery\n");
		control_mutex.lock();
		if(waiting)
		{
			control_wait.wakeOne();
			control_mutex.unlock();
		}
		else
		{
			control_mutex.unlock();
			KeyStoreManager *ksm = keyStoreManager();
			emit ksm->busyFinished();
		}
	}
};

//----------------------------------------------------------------------------
// KeyStoreManager
//----------------------------------------------------------------------------
KeyStoreManagerPrivate::KeyStoreManagerPrivate(KeyStoreManager *_q)
:q(_q)
{
	thread = 0;
}

KeyStoreManagerPrivate::~KeyStoreManagerPrivate()
{
	delete thread;
}

KeyStoreManager::KeyStoreManager()
{
	d = new KeyStoreManagerPrivate(this);
}

KeyStoreManager::~KeyStoreManager()
{
	delete d;
}

void KeyStoreManager::start()
{
	if(d->thread)
		return;

	moveToThread(QCoreApplication::instance()->thread());
	d->thread = new KeyStoreThread(this);
	d->thread->start();
	scan();
}

void KeyStoreManager::start(const QString &provider)
{
	Q_UNUSED(provider);
	start();
}

bool KeyStoreManager::isBusy() const
{
	return !(trackerInstance->hasInitialDiscovery());
}

void KeyStoreManager::waitForBusyFinished()
{
	d->thread->waitForInitialDiscovery();
}

QStringList KeyStoreManager::keyStores() const
{
	if(trackerInstance)
	{
		QMutexLocker locker(keyStoreMutex);
		QStringList out;
		for(int n = 0; n < trackerInstance->stores.count(); ++n)
		{
			KeyStoreTracker::Item *i = trackerInstance->stores[n];
			out += i->storeId;
		}
		return out;
	}
	else
		return QStringList();
}

int KeyStoreManager::count() const
{
	if(trackerInstance)
	{
		QMutexLocker locker(keyStoreMutex);
		return trackerInstance->stores.count();
	}
	else
		return 0;
}

QString KeyStoreManager::diagnosticText() const
{
	return d->dtext;
}

void KeyStoreManager::clearDiagnosticText()
{
	d->dtext = QString();
}

void KeyStoreManager::scan() const
{
	scanForPlugins();
	d->thread->scan();
}

}

#include "qca_keystore.moc"
