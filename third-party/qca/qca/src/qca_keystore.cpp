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

#include "qca_keystore.h"

#include <QCoreApplication>
#include <QAbstractEventDispatcher>
#include <QPointer>
#include <QSet>
#include <QMutex>
#include <QWaitCondition>

#include <stdlib.h> // abort
#include <stdio.h>  // fprintf

#include "qcaprovider.h"

Q_DECLARE_METATYPE(QCA::KeyStoreEntry)
Q_DECLARE_METATYPE(QList<QCA::KeyStoreEntry>)
Q_DECLARE_METATYPE(QList<QCA::KeyStoreEntry::Type>)
Q_DECLARE_METATYPE(QCA::KeyBundle)
Q_DECLARE_METATYPE(QCA::Certificate)
Q_DECLARE_METATYPE(QCA::CRL)
Q_DECLARE_METATYPE(QCA::PGPKey)

namespace QCA {

Provider::Context *getContext(const QString &type, Provider *p);

// from qca_plugin.cpp
QString truncate_log(const QString &in, int size);

/*
  How this stuff works:

  KeyStoreListContext is queried for a list of store context ids.  A signal
  is used to indicate when the list may have changed, so polling for changes
  is not necessary.  Context ids change for every new presence of a store.
  Even if a user removes and inserts the same smart card device, which has
  the same storeId, the context id will ALWAYS be different.  If a previously
  known context id is missing from a later queried list, then it means the
  associated store is unavailable.  It is recommended that the provider just
  use a counter for the contextId, incrementing the value anytime a new
  context is made.

  KeyStoreTracker manages all of the keystore stuff, and exists in its own
  thread (called the tracker thread).  All of the KeyStoreListContext
  objects exist in the tracker thread.
*/

/*
  scenarios to handle:
  - ksm.start shouldn't block
  - keystore in available list, but gone by the time it is requested
  - keystore is unavailable during a call to a keystoreentry method
  - keystore/keystoreentry methods called simultaneously from different threads
  - and of course, objects from keystores should work, despite being created
    in the keystore thread
*/

//----------------------------------------------------------------------------
// KeyStoreTracker
//----------------------------------------------------------------------------
static int tracker_id_at = 0;

class KeyStoreTracker : public QObject
{
	Q_OBJECT
public:
	static KeyStoreTracker *self;

	class Item
	{
	public:
		// combine keystore owner and contextid into a single id
		int trackerId;

		// number of times the keystore has been updated
		int updateCount;

		// keystore context
		KeyStoreListContext *owner;
		int storeContextId;

		// properties
		QString storeId;
		QString name;
		KeyStore::Type type;
		bool isReadOnly;

		Item() : trackerId(-1), updateCount(0), owner(0), storeContextId(-1)
		{
		}
	};

	QMutex m;
	QSet<KeyStoreListContext*> sources;
	QSet<KeyStoreListContext*> busySources;
	QList<Item> items;
	QString dtext;
	bool startedAll;
	bool busy;

	QMutex updateMutex;

	KeyStoreTracker()
	{
		self = this;

		qRegisterMetaType<KeyStoreEntry>();
		qRegisterMetaType< QList<KeyStoreEntry> >();
		qRegisterMetaType< QList<KeyStoreEntry::Type> >();
		qRegisterMetaType<KeyBundle>();
		qRegisterMetaType<Certificate>();
		qRegisterMetaType<CRL>();
		qRegisterMetaType<PGPKey>();

		connect(this, SIGNAL(updated_p()), SLOT(updated_locked()), Qt::QueuedConnection);

		startedAll = false;
		busy = true; // we start out busy
	}

	~KeyStoreTracker()
	{
		qDeleteAll(sources);
		self = 0;
	}

	static KeyStoreTracker *instance()
	{
		return self;
	}

	// thread-safe
	bool isBusy()
	{
		QMutexLocker locker(&m);
		return busy;
	}

	// thread-safe
	QList<Item> getItems()
	{
		QMutexLocker locker(&m);
		return items;
	}

	// thread-safe
	QString getDText()
	{
		QMutexLocker locker(&m);
		return dtext;
	}

	// thread-safe
	void clearDText()
	{
		QMutexLocker locker(&m);
		dtext.clear();
	}

	// thread-safe
	void addTarget(QObject *ksm)
	{
		QMutexLocker locker(&updateMutex);
		ksm->connect(this, SIGNAL(updated()), SLOT(tracker_updated()), Qt::DirectConnection);
	}

	// thread-safe
	void removeTarget(QObject *ksm)
	{
		QMutexLocker locker(&updateMutex);
		disconnect(ksm);
	}

public slots:
	void spinEventLoop()
	{
		QAbstractEventDispatcher::instance()->processEvents(QEventLoop::AllEvents);
	}

	void start()
	{
		// grab providers (and default)
		ProviderList list = providers();
		list.append(defaultProvider());

		for(int n = 0; n < list.count(); ++n)
		{
			Provider *p = list[n];
			if(p->features().contains("keystorelist") && !haveProviderSource(p))
				startProvider(p);
		}

		startedAll = true;
	}

	void start(const QString &provider)
	{
		// grab providers (and default)
		ProviderList list = providers();
		list.append(defaultProvider());

		Provider *p = 0;
		for(int n = 0; n < list.count(); ++n)
		{
			if(list[n]->name() == provider)
			{
				p = list[n];
				break;
			}
		}

		if(p && p->features().contains("keystorelist") && !haveProviderSource(p))
			startProvider(p);
	}

	void scan()
	{
		if(startedAll)
			start();
	}

	QList<QCA::KeyStoreEntry> entryList(int trackerId)
	{
		QList<KeyStoreEntry> out;
		int at = findItem(trackerId);
		if(at == -1)
			return out;
		Item &i = items[at];
		QList<KeyStoreEntryContext*> list = i.owner->entryList(i.storeContextId);
		for(int n = 0; n < list.count(); ++n)
		{
			KeyStoreEntry entry;
			entry.change(list[n]);
			out.append(entry);
		}
		return out;
	}

	QList<QCA::KeyStoreEntry::Type> entryTypes(int trackerId)
	{
		QList<KeyStoreEntry::Type> out;
		int at = findItem(trackerId);
		if(at == -1)
			return out;
		Item &i = items[at];
		return i.owner->entryTypes(i.storeContextId);
	}

	// hack with void *
	void *entry(const QString &storeId, const QString &entryId)
	{
		KeyStoreListContext *c = 0;
		int contextId = -1;
		m.lock();
		foreach(const Item &i, items)
		{
			if(i.storeId == storeId)
			{
				c = i.owner;
				contextId = i.storeContextId;
				break;
			}
		}
		m.unlock();
		if(!c)
			return 0;

		return c->entry(contextId, entryId);
	}

	// hack with void *
	void *entryPassive(const QString &serialized)
	{
		foreach(KeyStoreListContext *ksl, sources)
		{
			// "is this yours?"
			KeyStoreEntryContext *e = ksl->entryPassive(serialized);
			if(e)
				return e;
		}
		return 0;
	}

	QString writeEntry(int trackerId, const QVariant &v)
	{
		int at = findItem(trackerId);
		if(at == -1)
			return QString();
		Item &i = items[at];
		if(qVariantCanConvert<KeyBundle>(v))
			return i.owner->writeEntry(i.storeContextId, qVariantValue<KeyBundle>(v));
		else if(qVariantCanConvert<Certificate>(v))
			return i.owner->writeEntry(i.storeContextId, qVariantValue<Certificate>(v));
		else if(qVariantCanConvert<CRL>(v))
			return i.owner->writeEntry(i.storeContextId, qVariantValue<CRL>(v));
		else if(qVariantCanConvert<PGPKey>(v))
			return i.owner->writeEntry(i.storeContextId, qVariantValue<PGPKey>(v));
		else
			return QString();
	}

	QString writeEntry(int trackerId, const QCA::KeyBundle &v)
	{
		int at = findItem(trackerId);
		if(at == -1)
			return QString();
		Item &i = items[at];

		return i.owner->writeEntry(i.storeContextId, v);
	}

	QString writeEntry(int trackerId, const QCA::Certificate &v)
	{
		int at = findItem(trackerId);
		if(at == -1)
			return QString();
		Item &i = items[at];

		return i.owner->writeEntry(i.storeContextId, v);
	}

	QString writeEntry(int trackerId, const QCA::CRL &v)
	{
		int at = findItem(trackerId);
		if(at == -1)
			return QString();
		Item &i = items[at];

		return i.owner->writeEntry(i.storeContextId, v);
	}

	QString writeEntry(int trackerId, const QCA::PGPKey &v)
	{
		int at = findItem(trackerId);
		if(at == -1)
			return QString();
		Item &i = items[at];

		return i.owner->writeEntry(i.storeContextId, v);
	}

	bool removeEntry(int trackerId, const QString &entryId)
	{
		int at = findItem(trackerId);
		if(at == -1)
			return false;
		Item &i = items[at];
		return i.owner->removeEntry(i.storeContextId, entryId);
	}

signals:
	// emit this when items or busy state changes
	void updated();
	void updated_p();

private slots:
	void updated_locked()
	{
		QMutexLocker locker(&updateMutex);
		emit updated();
	}

private:
	bool haveProviderSource(Provider *p) const
	{
		foreach(KeyStoreListContext *ksl, sources)
		{
			if(ksl->provider() == p)
				return true;
		}
		return false;
	}

	int findItem(int trackerId)
	{
		for(int n = 0; n < items.count(); ++n)
		{
			if(items[n].trackerId == trackerId)
				return n;
		}
		return -1;
	}

	void startProvider(Provider *p)
	{
		KeyStoreListContext *c = static_cast<KeyStoreListContext *>(getContext("keystorelist", p));
		if(!c)
			return;

		sources += c;
		busySources += c;
		connect(c, SIGNAL(busyStart()), SLOT(ksl_busyStart()));
		connect(c, SIGNAL(busyEnd()), SLOT(ksl_busyEnd()));
		connect(c, SIGNAL(updated()), SLOT(ksl_updated()));
		connect(c, SIGNAL(diagnosticText(const QString &)), SLOT(ksl_diagnosticText(const QString &)));
		connect(c, SIGNAL(storeUpdated(int)), SLOT(ksl_storeUpdated(int)));
		c->start();
		c->setUpdatesEnabled(true);

		QCA_logTextMessage(QString("keystore: startProvider %1").arg(p->name()), Logger::Information);
	}

	bool updateStores(KeyStoreListContext *c)
	{
		bool changed = false;

		QMutexLocker locker(&m);

		QList<int> keyStores = c->keyStores();

		// remove any contexts that are gone
		for(int n = 0; n < items.count(); ++n)
		{
			if(items[n].owner == c && !keyStores.contains(items[n].storeContextId))
			{
				QCA_logTextMessage(QString("keystore: updateStores remove %1").arg(items[n].storeContextId), Logger::Information);

				items.removeAt(n);
				--n; // adjust position

				changed = true;
			}
		}

		// handle add/updates
		foreach(int id, keyStores)
		{
			// do we have it already?
			int at = -1;
			for(int n = 0; n < items.count(); ++n)
			{
				if(items[n].owner == c && items[n].storeContextId == id)
				{
					at = n;
					break;
				}
			}

			// if so, update it
			if(at != -1)
			{
				Item &i = items[at];

				QString name = c->name(id);
				bool isReadOnly = c->isReadOnly(id);
				if(i.name != name || i.isReadOnly != isReadOnly)
				{
					QCA_logTextMessage(QString("keystore: updateStores update %1").arg(id), Logger::Information);
					i.name = name;
					i.isReadOnly = isReadOnly;
					changed = true;
				}
			}
			// otherwise, add it
			else
			{
				QCA_logTextMessage(QString("keystore: updateStores add %1").arg(id), Logger::Information);

				Item i;
				i.trackerId = tracker_id_at++;
				i.updateCount = 0;
				i.owner = c;
				i.storeContextId = id;
				i.storeId = c->storeId(id);
				i.name = c->name(id);
				i.type = c->type(id);
				i.isReadOnly = c->isReadOnly(id);
				items += i;

				changed = true;
			}
		}

		return changed;
	}

private slots:
	void ksl_busyStart()
	{
		KeyStoreListContext *c = (KeyStoreListContext *)sender();

		QCA_logTextMessage(QString("keystore: ksl_busyStart %1").arg(c->provider()->name()), Logger::Information);

		if(!busySources.contains(c))
		{
			busySources += c;

			QCA_logTextMessage(QString("keystore: emitting updated"), Logger::Information);
			emit updated_p();
		}
	}

	void ksl_busyEnd()
	{
		KeyStoreListContext *c = (KeyStoreListContext *)sender();

		QCA_logTextMessage(QString("keystore: ksl_busyEnd %1").arg(c->provider()->name()), Logger::Information);

		busySources.remove(c);
		bool changed = updateStores(c);
		bool any_busy = !busySources.isEmpty();

		if(!any_busy)
		{
			m.lock();
			busy = false;
			m.unlock();
		}

		if(!any_busy || changed)
		{
			QCA_logTextMessage(QString("keystore: emitting updated"), Logger::Information);
			emit updated_p();
		}
	}

	void ksl_updated()
	{
		KeyStoreListContext *c = (KeyStoreListContext *)sender();

		QCA_logTextMessage(QString("keystore: ksl_updated %1").arg(c->provider()->name()), Logger::Information);

		bool changed = updateStores(c);
		if(changed)
		{
			QCA_logTextMessage(QString("keystore: emitting updated"), Logger::Information);
			emit updated_p();
		}
	}

	void ksl_diagnosticText(const QString &str)
	{
		QMutexLocker locker(&m);
		dtext += str;
		dtext = truncate_log(dtext, 100000);
	}

	void ksl_storeUpdated(int id)
	{
		KeyStoreListContext *c = (KeyStoreListContext *)sender();

		QCA_logTextMessage(QString("keystore: ksl_storeUpdated %1 %2").arg(c->provider()->name(), QString::number(id)), Logger::Information);

		QMutexLocker locker(&m);
		for(int n = 0; n < items.count(); ++n)
		{
			Item &i = items[n];
			if(i.owner == c && i.storeContextId == id)
			{
				++i.updateCount;

				QCA_logTextMessage(QString("keystore: %1 updateCount = %2").arg(i.name, QString::number(i.updateCount)), Logger::Information);

				QCA_logTextMessage(QString("keystore: emitting updated"), Logger::Information);
				emit updated_p();
				return;
			}
		}
	}
};

KeyStoreTracker *KeyStoreTracker::self = 0;

//----------------------------------------------------------------------------
// KeyStoreThread
//----------------------------------------------------------------------------
class KeyStoreThread : public SyncThread
{
	Q_OBJECT
public:
	KeyStoreTracker *tracker;
	QMutex call_mutex;

	KeyStoreThread(QObject *parent = 0) : SyncThread(parent)
	{
	}

	~KeyStoreThread()
	{
		stop();
	}

	void atStart()
	{
		tracker = new KeyStoreTracker;
	}

	void atEnd()
	{
		delete tracker;
	}
};

//----------------------------------------------------------------------------
// KeyStoreGlobal
//----------------------------------------------------------------------------
class KeyStoreManagerGlobal;

Q_GLOBAL_STATIC(QMutex, ksm_mutex)
static KeyStoreManagerGlobal *g_ksm = 0;

class KeyStoreManagerGlobal
{
public:
	KeyStoreThread *thread;

	KeyStoreManagerGlobal()
	{
		thread = new KeyStoreThread;
		thread->moveToThread(QCoreApplication::instance()->thread());
		thread->start();
	}

	~KeyStoreManagerGlobal()
	{
		delete thread;
	}
};

// this function is thread-safe
static QVariant trackercall(const char *method, const QVariantList &args = QVariantList())
{
	QVariant ret;
	bool ok;

	g_ksm->thread->call_mutex.lock();
	ret = g_ksm->thread->call(KeyStoreTracker::instance(), method, args, &ok);
	g_ksm->thread->call_mutex.unlock();

	Q_ASSERT(ok);
	if(!ok)
	{
		fprintf(stderr, "QCA: KeyStoreTracker call [%s] failed.\n", method);
		abort();
		return QVariant();
	}
	return ret;
}

//----------------------------------------------------------------------------
// KeyStoreEntry
//----------------------------------------------------------------------------
class KeyStoreEntry::Private
{
public:
	bool accessible;

	Private()
	{
		accessible = false;
	}
};

KeyStoreEntry::KeyStoreEntry()
:d(new Private)
{
}

KeyStoreEntry::KeyStoreEntry(const QString &serialized)
:d(new Private)
{
	*this = fromString(serialized);
}

KeyStoreEntry::KeyStoreEntry(const KeyStoreEntry &from)
:Algorithm(from), d(new Private(*from.d))
{
}

KeyStoreEntry::~KeyStoreEntry()
{
	delete d;
}

KeyStoreEntry & KeyStoreEntry::operator=(const KeyStoreEntry &from)
{
	Algorithm::operator=(from);
	*d = *from.d;
	return *this;
}

bool KeyStoreEntry::isNull() const
{
	return (!context() ? true : false);
}

bool KeyStoreEntry::isAvailable() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->isAvailable();
}

bool KeyStoreEntry::isAccessible() const
{
	return d->accessible;
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

QString KeyStoreEntry::storeName() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->storeName();
}

QString KeyStoreEntry::storeId() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->storeId();
}

QString KeyStoreEntry::toString() const
{
	return static_cast<const KeyStoreEntryContext *>(context())->serialize();
}

KeyStoreEntry KeyStoreEntry::fromString(const QString &serialized)
{
	KeyStoreEntry e;
	KeyStoreEntryContext *c = (KeyStoreEntryContext *)KeyStoreTracker::instance()->entryPassive(serialized);
	if(c)
		e.change(c);
	return e;
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

bool KeyStoreEntry::ensureAvailable()
{
	QString storeId = this->storeId();
	QString entryId = id();
	KeyStoreEntryContext *c = (KeyStoreEntryContext *)qVariantValue<void*>(trackercall("entry", QVariantList() << storeId << entryId));
	if(c)
		change(c);
	return isAvailable();
}

bool KeyStoreEntry::ensureAccess()
{
	if(!ensureAvailable())
	{
		d->accessible = false;
		return false;
	}
	bool ok = static_cast<KeyStoreEntryContext *>(context())->ensureAccess();
	d->accessible = ok;
	return d->accessible;
}

//----------------------------------------------------------------------------
// KeyStoreEntryWatcher
//----------------------------------------------------------------------------
class KeyStoreEntryWatcher::Private : public QObject
{
	Q_OBJECT
public:
	KeyStoreEntryWatcher *q;
	KeyStoreManager ksm;
	KeyStoreEntry entry;
	QString storeId, entryId;
	KeyStore *ks;
	bool avail;

	Private(KeyStoreEntryWatcher *_q) : QObject(_q), q(_q), ksm(this)
	{
		ks = 0;
		avail = false;
		connect(&ksm, SIGNAL(keyStoreAvailable(const QString &)), SLOT(ksm_available(const QString &)));
	}

	~Private()
	{
		delete ks;
	}

	void start()
	{
		QStringList list = ksm.keyStores();
		foreach(const QString &storeId, list)
			ksm_available(storeId);
	}

private slots:
	void ksm_available(const QString &_storeId)
	{
		// we only care about one store
		if(_storeId == storeId)
		{
			ks = new KeyStore(storeId, &ksm);
			connect(ks, SIGNAL(updated()), SLOT(ks_updated()));
			ks->startAsynchronousMode();
		}
	}

	void ks_updated()
	{
		bool found = false;
		QList<KeyStoreEntry> list = ks->entryList();
		foreach(const KeyStoreEntry &e, list)
		{
			if(e.id() == entryId && e.isAvailable())
			{
				found = true;
				if(!avail)
					entry = e;
				break;
			}
		}

		if(found && !avail)
		{
			avail = true;
			emit q->available();
		}
		else if(!found && avail)
		{
			avail = false;
			emit q->unavailable();
		}
	}

	void ks_unavailable()
	{
		delete ks;
		ks = 0;

		if(avail)
		{
			avail = false;
			emit q->unavailable();
		}
	}
};

KeyStoreEntryWatcher::KeyStoreEntryWatcher(const KeyStoreEntry &e, QObject *parent)
:QObject(parent)
{
	d = new Private(this);
	if(!e.isNull())
	{
		d->entry = e;
		d->storeId = e.storeId();
		d->entryId = e.id();
		d->start();
	}
}

KeyStoreEntryWatcher::~KeyStoreEntryWatcher()
{
	delete d;
}

KeyStoreEntry KeyStoreEntryWatcher::entry() const
{
	return d->entry;
}

//----------------------------------------------------------------------------
// KeyStore
//----------------------------------------------------------------------------
// union thingy
class KeyStoreWriteEntry
{
public:
	enum Type { TypeKeyBundle, TypeCertificate, TypeCRL, TypePGPKey };

	Type type;
	KeyBundle keyBundle;
	Certificate cert;
	CRL crl;
	PGPKey pgpKey;

	KeyStoreWriteEntry()
	{
	}

	KeyStoreWriteEntry(const KeyBundle &_keyBundle)
	:type(TypeKeyBundle), keyBundle(_keyBundle)
	{
	}

	KeyStoreWriteEntry(const Certificate &_cert)
	:type(TypeCertificate), cert(_cert)
	{
	}

	KeyStoreWriteEntry(const CRL &_crl)
	:type(TypeCRL), crl(_crl)
	{
	}

	KeyStoreWriteEntry(const PGPKey &_pgpKey)
	:type(TypePGPKey), pgpKey(_pgpKey)
	{
	}
};

class KeyStoreOperation : public QThread
{
	Q_OBJECT
public:
	enum Type { EntryList, WriteEntry, RemoveEntry };

	Type type;
	int trackerId;

	KeyStoreWriteEntry wentry; // in: WriteEntry
	QList<KeyStoreEntry> entryList; // out: EntryList
	QString entryId; // in: RemoveEntry, out: WriteEntry
	bool success; // out: RemoveEntry

	KeyStoreOperation(QObject *parent = 0)
	:QThread(parent)
	{
	}

	~KeyStoreOperation()
	{
		wait();
	}

protected:
	virtual void run()
	{
		if(type == EntryList)
			entryList = qVariantValue< QList<KeyStoreEntry> >(trackercall("entryList", QVariantList() << trackerId));
		else if(type == WriteEntry)
		{
			QVariant arg;
			if(wentry.type == KeyStoreWriteEntry::TypeKeyBundle)
				qVariantSetValue<KeyBundle>(arg, wentry.keyBundle);
			else if(wentry.type == KeyStoreWriteEntry::TypeCertificate)
				qVariantSetValue<Certificate>(arg, wentry.cert);
			else if(wentry.type == KeyStoreWriteEntry::TypeCRL)
				qVariantSetValue<CRL>(arg, wentry.crl);
			else if(wentry.type == KeyStoreWriteEntry::TypePGPKey)
				qVariantSetValue<PGPKey>(arg, wentry.pgpKey);

			// note: each variant in the argument list is resolved
			//   to its native type.  so even though it looks like
			//   we're attempting to call a method named
			//   writeEntry(QString,QVariant), we're actually
			//   calling one of many possible methods, such as
			//   writeEntry(QString,PGPKey) or
			//   writeEntry(QString,Certificate), etc, depending
			//   on the type of object we put in the variant.
			entryId = trackercall("writeEntry", QVariantList() << trackerId << arg).toString();
		}
		else // RemoveEntry
		{
			success = trackercall("removeEntry", QVariantList() << trackerId << entryId).toBool();
		}
	}
};

class KeyStorePrivate : public QObject
{
	Q_OBJECT
public:
	KeyStore *q;
	KeyStoreManager *ksm;
	int trackerId;
	KeyStoreTracker::Item item;
	bool async;
	bool need_update;
	QList<KeyStoreEntry> latestEntryList;
	QList<KeyStoreOperation*> ops;

	KeyStorePrivate(KeyStore *_q) : QObject(_q), q(_q), async(false)
	{
	}

	~KeyStorePrivate()
	{
		qDeleteAll(ops);
	}

	// implemented below, after KeyStorePrivate is declared
	void reg();
	void unreg();
	KeyStoreTracker::Item *getItem(const QString &storeId);
	KeyStoreTracker::Item *getItem(int trackerId);

	void invalidate()
	{
		trackerId = -1;
		unreg();
	}

	bool have_entryList_op() const
	{
		foreach(KeyStoreOperation *op, ops)
		{
			if(op->type == KeyStoreOperation::EntryList)
				return true;
		}
		return false;
	}

	void handle_updated()
	{
		if(async)
		{
			if(!have_entryList_op())
				async_entryList();
			else
				need_update = true;
		}
		else
			emit q->updated();
	}

	void async_entryList()
	{
		KeyStoreOperation *op = new KeyStoreOperation(this);
		// use queued for signal-safety
		connect(op, SIGNAL(finished()), SLOT(op_finished()), Qt::QueuedConnection);
		op->type = KeyStoreOperation::EntryList;
		op->trackerId = trackerId;
		ops += op;
		op->start();
	}

	void async_writeEntry(const KeyStoreWriteEntry &wentry)
	{
		KeyStoreOperation *op = new KeyStoreOperation(this);
		// use queued for signal-safety
		connect(op, SIGNAL(finished()), SLOT(op_finished()), Qt::QueuedConnection);
		op->type = KeyStoreOperation::WriteEntry;
		op->trackerId = trackerId;
		op->wentry = wentry;
		ops += op;
		op->start();
	}

	void async_removeEntry(const QString &entryId)
	{
		KeyStoreOperation *op = new KeyStoreOperation(this);
		// use queued for signal-safety
		connect(op, SIGNAL(finished()), SLOT(op_finished()), Qt::QueuedConnection);
		op->type = KeyStoreOperation::RemoveEntry;
		op->trackerId = trackerId;
		op->entryId = entryId;
		ops += op;
		op->start();
	}

private slots:
	void op_finished()
	{
		KeyStoreOperation *op = (KeyStoreOperation *)sender();

		if(op->type == KeyStoreOperation::EntryList)
		{
			latestEntryList = op->entryList;
			ops.removeAll(op);
			delete op;

			if(need_update)
			{
				need_update = false;
				async_entryList();
			}

			emit q->updated();
		}
		else if(op->type == KeyStoreOperation::WriteEntry)
		{
			QString entryId = op->entryId;
			ops.removeAll(op);
			delete op;

			emit q->entryWritten(entryId);
		}
		else // RemoveEntry
		{
			bool success = op->success;
			ops.removeAll(op);
			delete op;

			emit q->entryRemoved(success);
		}
	}
};

KeyStore::KeyStore(const QString &id, KeyStoreManager *keyStoreManager)
:QObject(keyStoreManager)
{
	d = new KeyStorePrivate(this);
	d->ksm = keyStoreManager;

	KeyStoreTracker::Item *i = d->getItem(id);
	if(i)
	{
		d->trackerId = i->trackerId;
		d->item = *i;
		d->reg();
	}
	else
		d->trackerId = -1;
}

KeyStore::~KeyStore()
{
	if(d->trackerId != -1)
		d->unreg();
	delete d;
}

bool KeyStore::isValid() const
{
	return (d->getItem(d->trackerId) ? true : false);
}

KeyStore::Type KeyStore::type() const
{
	return d->item.type;
}

QString KeyStore::name() const
{
	return d->item.name;
}

QString KeyStore::id() const
{
	return d->item.storeId;
}

bool KeyStore::isReadOnly() const
{
	return d->item.isReadOnly;
}

void KeyStore::startAsynchronousMode()
{
	if(d->async)
		return;

	d->async = true;

	// initial entrylist
	d->need_update = false;
	d->async_entryList();
}

QList<KeyStoreEntry> KeyStore::entryList() const
{
	if(d->async)
		return d->latestEntryList;

	if(d->trackerId == -1)
		return QList<KeyStoreEntry>();
	return qVariantValue< QList<KeyStoreEntry> >(trackercall("entryList", QVariantList() << d->trackerId));
}

bool KeyStore::holdsTrustedCertificates() const
{
	QList<KeyStoreEntry::Type> list;
	if(d->trackerId == -1)
		return false;
	list = qVariantValue< QList<KeyStoreEntry::Type> >(trackercall("entryTypes", QVariantList() << d->trackerId));
	if(list.contains(KeyStoreEntry::TypeCertificate) || list.contains(KeyStoreEntry::TypeCRL))
		return true;
	return false;
}

bool KeyStore::holdsIdentities() const
{
	QList<KeyStoreEntry::Type> list;
	if(d->trackerId == -1)
		return false;
	list = qVariantValue< QList<KeyStoreEntry::Type> >(trackercall("entryTypes", QVariantList() << d->trackerId));
	if(list.contains(KeyStoreEntry::TypeKeyBundle) || list.contains(KeyStoreEntry::TypePGPSecretKey))
		return true;
	return false;
}

bool KeyStore::holdsPGPPublicKeys() const
{
	QList<KeyStoreEntry::Type> list;
	if(d->trackerId == -1)
		return false;
	list = qVariantValue< QList<KeyStoreEntry::Type> >(trackercall("entryTypes", QVariantList() << d->trackerId));
	if(list.contains(KeyStoreEntry::TypePGPPublicKey))
		return true;
	return false;
}

QString KeyStore::writeEntry(const KeyBundle &kb)
{
	if(d->async)
	{
		d->async_writeEntry(KeyStoreWriteEntry(kb));
		return QString();
	}
	else
	{
		QVariant arg;
		qVariantSetValue<KeyBundle>(arg, kb);
		return trackercall("writeEntry", QVariantList() << d->trackerId << arg).toString();
	}
}

QString KeyStore::writeEntry(const Certificate &cert)
{
	if(d->async)
	{
		d->async_writeEntry(KeyStoreWriteEntry(cert));
		return QString();
	}
	else
	{
		QVariant arg;
		qVariantSetValue<Certificate>(arg, cert);
		return trackercall("writeEntry", QVariantList() << d->trackerId << arg).toString();
	}
}

QString KeyStore::writeEntry(const CRL &crl)
{
	if(d->async)
	{
		d->async_writeEntry(KeyStoreWriteEntry(crl));
		return QString();
	}
	else
	{
		QVariant arg;
		qVariantSetValue<CRL>(arg, crl);
		return trackercall("writeEntry", QVariantList() << d->trackerId << arg).toString();
	}
}

QString KeyStore::writeEntry(const PGPKey &key)
{
	if(d->async)
	{
		d->async_writeEntry(KeyStoreWriteEntry(key));
		return QString();
	}
	else
	{
		QVariant arg;
		qVariantSetValue<PGPKey>(arg, key);
		return trackercall("writeEntry", QVariantList() << d->trackerId << arg).toString();
	}
}

bool KeyStore::removeEntry(const QString &id)
{
	if(d->async)
	{
		d->async_removeEntry(id);
		return false;
	}
	else
	{
		return trackercall("removeEntry", QVariantList() << d->trackerId << id).toBool();
	}
}

//----------------------------------------------------------------------------
// KeyStoreManager
//----------------------------------------------------------------------------
static void ensure_init()
{
	QMutexLocker locker(ksm_mutex());
	if(!g_ksm)
		g_ksm = new KeyStoreManagerGlobal;
}

// static functions
void KeyStoreManager::start()
{
	ensure_init();
	QMetaObject::invokeMethod(KeyStoreTracker::instance(), "start", Qt::QueuedConnection);
	trackercall("spinEventLoop");
}

void KeyStoreManager::start(const QString &provider)
{
	ensure_init();
	QMetaObject::invokeMethod(KeyStoreTracker::instance(), "start", Qt::QueuedConnection, Q_ARG(QString, provider));
	trackercall("spinEventLoop");
}

QString KeyStoreManager::diagnosticText()
{
	ensure_init();

	// spin one event cycle in the tracker, to receive any pending text.
	//   note that since trackercall also goes through the eventloop,
	//   this may end up doing two rounds.  probably no big deal.
	trackercall("spinEventLoop");

	return KeyStoreTracker::instance()->getDText();
}

void KeyStoreManager::clearDiagnosticText()
{
	ensure_init();
	KeyStoreTracker::instance()->clearDText();
}

void KeyStoreManager::scan()
{
	ensure_init();
	QMetaObject::invokeMethod(KeyStoreTracker::instance(), "scan", Qt::QueuedConnection);
}

void KeyStoreManager::shutdown()
{
	QMutexLocker locker(ksm_mutex());
	delete g_ksm;
	g_ksm = 0;
}

// object
class KeyStoreManagerPrivate : public QObject
{
	Q_OBJECT
public:
	KeyStoreManager *q;

	QMutex m;
	QWaitCondition w;
	bool busy;
	QList<KeyStoreTracker::Item> items;
	bool pending, waiting;

	QMultiHash<int,KeyStore*> keyStoreForTrackerId;
	QHash<KeyStore*,int> trackerIdForKeyStore;

	KeyStoreManagerPrivate(KeyStoreManager *_q) : QObject(_q), q(_q)
	{
		pending = false;
		waiting = false;
	}

	~KeyStoreManagerPrivate()
	{
		// invalidate registered keystores
		QList<KeyStore*> list;
		QHashIterator<KeyStore*,int> it(trackerIdForKeyStore);
		while(it.hasNext())
		{
			it.next();
			list += it.key();
		}
		foreach(KeyStore *ks, list)
			ks->d->invalidate();
	}

	// for keystore
	void reg(KeyStore *ks, int trackerId)
	{
		keyStoreForTrackerId.insert(trackerId, ks);
		trackerIdForKeyStore.insert(ks, trackerId);
	}

	void unreg(KeyStore *ks)
	{
		int trackerId = trackerIdForKeyStore.take(ks);

		// this is the only way I know to remove one item from a multihash
		QList<KeyStore*> vals = keyStoreForTrackerId.values(trackerId);
		keyStoreForTrackerId.remove(trackerId);
		vals.removeAll(ks);
		foreach(KeyStore *i, vals)
			keyStoreForTrackerId.insert(trackerId, i);
	}

	KeyStoreTracker::Item *getItem(const QString &storeId)
	{
		for(int n = 0; n < items.count(); ++n)
		{
			KeyStoreTracker::Item *i = &items[n];
			if(i->storeId == storeId)
				return i;
		}
		return 0;
	}

	KeyStoreTracker::Item *getItem(int trackerId)
	{
		for(int n = 0; n < items.count(); ++n)
		{
			KeyStoreTracker::Item *i = &items[n];
			if(i->trackerId == trackerId)
				return i;
		}
		return 0;
	}

	void do_update()
	{
		// ksm doesn't have reset or state changes so we can
		//   use QPointer here for full SS.
		QPointer<QObject> self(this);

		bool newbusy = KeyStoreTracker::instance()->isBusy();
		QList<KeyStoreTracker::Item> newitems = KeyStoreTracker::instance()->getItems();

		if(!busy && newbusy)
		{
			emit q->busyStarted();
			if(!self)
				return;
		}
		if(busy && !newbusy)
		{
			emit q->busyFinished();
			if(!self)
				return;
		}

		QStringList here;
		QList<int> changed;
		QList<int> gone;

		// removed
		for(int n = 0; n < items.count(); ++n)
		{
			KeyStoreTracker::Item &i = items[n];
			bool found = false;
			for(int k = 0; k < newitems.count(); ++k)
			{
				if(i.trackerId == newitems[k].trackerId)
				{
					found = true;
					break;
				}
			}
			if(!found)
				gone += i.trackerId;
		}

		// changed
		for(int n = 0; n < items.count(); ++n)
		{
			KeyStoreTracker::Item &i = items[n];
			for(int k = 0; k < newitems.count(); ++k)
			{
				if(i.trackerId == newitems[k].trackerId)
				{
					if(i.updateCount < newitems[k].updateCount)
						changed += i.trackerId;
					break;
				}
			}
		}

		// added
		for(int n = 0; n < newitems.count(); ++n)
		{
			KeyStoreTracker::Item &i = newitems[n];
			bool found = false;
			for(int k = 0; k < items.count(); ++k)
			{
				if(i.trackerId == items[k].trackerId)
				{
					found = true;
					break;
				}
			}
			if(!found)
				here += i.storeId;
		}

		busy = newbusy;
		items = newitems;

		// signals
		foreach(int trackerId, gone)
		{
			KeyStore *ks = keyStoreForTrackerId.value(trackerId);
			if(ks)
			{
				ks->d->invalidate();
				emit ks->unavailable();
				if(!self)
					return;
			}
		}

		foreach(int trackerId, changed)
		{
			KeyStore *ks = keyStoreForTrackerId.value(trackerId);
			if(ks)
			{
				ks->d->handle_updated();
				if(!self)
					return;
			}
		}

		foreach(const QString &storeId, here)
		{
			emit q->keyStoreAvailable(storeId);
			if(!self)
				return;
		}
	}

public slots:
	void tracker_updated()
	{
		QCA_logTextMessage(QString().sprintf("keystore: %p: tracker_updated start", q), Logger::Information);

		QMutexLocker locker(&m);
		if(!pending)
		{
			QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
			pending = true;
		}
		if(waiting && !KeyStoreTracker::instance()->isBusy())
		{
			busy = false;
			items = KeyStoreTracker::instance()->getItems();
			w.wakeOne();
		}

		QCA_logTextMessage(QString().sprintf("keystore: %p: tracker_updated end", q), Logger::Information);
	}

	void update()
	{
		m.lock();
		pending = false;
		m.unlock();

		do_update();
	}
};

// from KeyStorePrivate
void KeyStorePrivate::reg()
{
	ksm->d->reg(q, trackerId);
}

void KeyStorePrivate::unreg()
{
	ksm->d->unreg(q);
}

KeyStoreTracker::Item *KeyStorePrivate::getItem(const QString &storeId)
{
	return ksm->d->getItem(storeId);
}

KeyStoreTracker::Item *KeyStorePrivate::getItem(int trackerId)
{
	return ksm->d->getItem(trackerId);
}

KeyStoreManager::KeyStoreManager(QObject *parent)
:QObject(parent)
{
	ensure_init();
	d = new KeyStoreManagerPrivate(this);
	KeyStoreTracker::instance()->addTarget(d);
	sync();
}

KeyStoreManager::~KeyStoreManager()
{
	Q_ASSERT(KeyStoreTracker::instance());
	KeyStoreTracker::instance()->removeTarget(d);
	delete d;
}

bool KeyStoreManager::isBusy() const
{
	return d->busy;
}

void KeyStoreManager::waitForBusyFinished()
{
	d->m.lock();
	d->busy = KeyStoreTracker::instance()->isBusy();
	if(d->busy)
	{
		d->waiting = true;
		d->w.wait(&d->m);
		d->waiting = false;
	}
	d->m.unlock();
}

QStringList KeyStoreManager::keyStores() const
{
	QStringList out;
	for(int n = 0; n < d->items.count(); ++n)
		out += d->items[n].storeId;
	return out;
}

void KeyStoreManager::sync()
{
	d->busy = KeyStoreTracker::instance()->isBusy();
	d->items = KeyStoreTracker::instance()->getItems();
}

//----------------------------------------------------------------------------
// KeyStoreInfo
//----------------------------------------------------------------------------
class KeyStoreInfo::Private : public QSharedData
{
public:
	KeyStore::Type type;
	QString id, name;
};

KeyStoreInfo::KeyStoreInfo()
{
}

KeyStoreInfo::KeyStoreInfo(KeyStore::Type type, const QString &id, const QString &name)
:d(new Private)
{
	d->type = type;
	d->id = id;
	d->name = name;
}

KeyStoreInfo::KeyStoreInfo(const KeyStoreInfo &from)
:d(from.d)
{
}

KeyStoreInfo::~KeyStoreInfo()
{
}

KeyStoreInfo & KeyStoreInfo::operator=(const KeyStoreInfo &from)
{
	d = from.d;
	return *this;
}

bool KeyStoreInfo::isNull() const
{
	return (d ? false: true);
}

KeyStore::Type KeyStoreInfo::type() const
{
	return d->type;
}

QString KeyStoreInfo::id() const
{
	return d->id;
}

QString KeyStoreInfo::name() const
{
	return d->name;
}

}

#include "qca_keystore.moc"
