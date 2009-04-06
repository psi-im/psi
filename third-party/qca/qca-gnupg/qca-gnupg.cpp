/*
 * Copyright (C) 2003-2008  Justin Karneges <justin@affinix.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <QtCrypto>
#include <qcaprovider.h>
#include <QtPlugin>
#include <QTemporaryFile>
#include <QDir>
#include <QLatin1String>
#include <QMutex>
#include <stdio.h>

#ifdef Q_OS_MAC
#include <QFileInfo>
#endif

#ifdef Q_OS_WIN
# include<windows.h>
#endif
#include "gpgop.h"

// for SafeTimer and release function
#include "gpgproc.h"

using namespace QCA;

namespace gpgQCAPlugin {

#ifdef Q_OS_LINUX
static int qVersionInt()
{
	static int out = -1;

	if(out == -1)
	{
		QString str = QString::fromLatin1(qVersion());
		QStringList parts = str.split('.', QString::KeepEmptyParts);
		if(parts.count() != 3)
		{
			out = 0;
			return out;
		}

		out = 0;
		for(int n = 0; n < 3; ++n)
		{
			bool ok;
			int x = parts[n].toInt(&ok);
			if(ok && x >= 0 && x <= 0xff)
			{
				out <<= 8;
				out += x;
			}
			else
			{
				out = 0;
				return out;
			}
		}
	}

	return out;
}

static bool qt_buggy_fsw()
{
	// fixed in 4.3.5 and 4.4.1
	int ver = qVersionInt();
	int majmin = ver >> 8;
	if(majmin < 0x0403)
		return true;
	else if(majmin == 0x0403 && ver < 0x040305)
		return true;
	else if(majmin == 0x0404 && ver < 0x040401)
		return true;
	return false;
}
#else
static bool qt_buggy_fsw()
{
	return false;
}
#endif

// begin ugly hack for qca 2.0.0 with broken dirwatch support

// hacks:
//  1) we must construct with a valid file to watch.  passing an empty
//     string doesn't work.  this means we can't create the objects in
//     advance.  instead we'll use new/delete as necessary.
//  2) there's a wrong internal connect() statement in the qca source.
//     assuming fixed internals for qca 2.0.0, we can fix that connect from
//     here...

#include <QFileSystemWatcher>

// some structures below to give accessible interface to qca 2.0.0 internals

class DirWatch2 : public QObject
{
        Q_OBJECT
public:
        explicit DirWatch2(const QString &dir = QString(), QObject *parent = 0)
	{
		Q_UNUSED(dir);
		Q_UNUSED(parent);
	}

	~DirWatch2()
	{
	}

	QString dirName() const
	{
		return QString();
	}

	void setDirName(const QString &dir)
	{
		Q_UNUSED(dir);
	}

Q_SIGNALS:
	void changed();

public:
	Q_DISABLE_COPY(DirWatch2)

	class Private;
	friend class Private;
	Private *d;
};

/*class FileWatch2 : public QObject
{
	Q_OBJECT
public:
	explicit FileWatch2(const QString &file = QString(), QObject *parent = 0)
	{
		Q_UNUSED(file);
		Q_UNUSED(parent);
	}

	~FileWatch2()
	{
	}

	QString fileName() const
	{
		return QString();
	}

	void setFileName(const QString &file)
	{
		Q_UNUSED(file);
	}

Q_SIGNALS:
	void changed();

public:
	Q_DISABLE_COPY(FileWatch2)

	class Private;
	friend class Private;
	Private *d;
};*/

class QFileSystemWatcherRelay2 : public QObject
{
	Q_OBJECT
public:
};

class DirWatch2::Private : public QObject
{
        Q_OBJECT
public:
        DirWatch2 *q;
        QFileSystemWatcher *watcher;
        QFileSystemWatcherRelay2 *watcher_relay;
        QString dirName;

private slots:
	void watcher_changed(const QString &path)
	{
		Q_UNUSED(path);
	}
};

/*class FileWatch2::Private : public QObject
{
	Q_OBJECT
public:
	FileWatch2 *q;
	QFileSystemWatcher *watcher;
	QFileSystemWatcherRelay2 *watcher_relay;
	QString fileName;

private slots:
	void watcher_changed(const QString &path)
	{
		Q_UNUSED(path);
	}
};*/

static void hack_fix(DirWatch *dw)
{
	DirWatch2 *dw2 = reinterpret_cast<DirWatch2*>(dw);
	QObject::connect(dw2->d->watcher_relay, SIGNAL(directoryChanged(const QString &)), dw2->d, SLOT(watcher_changed(const QString &)));
	fprintf(stderr, "qca-gnupg: patching DirWatch to fix failed connect\n");
}

// end ugly hack

#ifdef Q_OS_WIN
static QString find_reg_gpgProgram()
{
	HKEY root;
	root = HKEY_CURRENT_USER;

	HKEY hkey;
	const char *path = "Software\\GNU\\GnuPG";
	if(RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
	{
		if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
			return QString();
	}

	char szValue[256];
	DWORD dwLen = 256;
	if(RegQueryValueExA(hkey, "gpgProgram", NULL, NULL, (LPBYTE)szValue, &dwLen) != ERROR_SUCCESS)
	{
		RegCloseKey(hkey);
		return QString();
	}

	RegCloseKey(hkey);
	return QString::fromLatin1(szValue);
}
#endif

static QString find_bin()
{
	QString bin = "gpg";
#ifdef Q_OS_WIN
	QString s = find_reg_gpgProgram();
	if(!s.isNull())
		bin = s;
#endif
#ifdef Q_OS_MAC
	// mac-gpg
	QFileInfo fi("/usr/local/bin/gpg");
	if(fi.exists())
		bin = fi.filePath();
#endif
	return bin;
}

static QString escape_string(const QString &in)
{
	QString out;
	for(int n = 0; n < in.length(); ++n)
	{
		if(in[n] == '\\')
			out += "\\\\";
		else if(in[n] == ':')
			out += "\\c";
		else
			out += in[n];
	}
	return out;
}

static QString unescape_string(const QString &in)
{
	QString out;
	for(int n = 0; n < in.length(); ++n)
	{
		if(in[n] == '\\')
		{
			if(n + 1 < in.length())
			{
				if(in[n + 1] == '\\')
					out += '\\';
				else if(in[n + 1] == 'c')
					out += ':';
				++n;
			}
		}
		else
			out += in[n];
	}
	return out;
}

static void gpg_waitForFinished(GpgOp *gpg)
{
	while(1)
	{
		GpgOp::Event e = gpg->waitForEvent(-1);
		if(e.type == GpgOp::Event::Finished)
			break;
	}
}

Q_GLOBAL_STATIC(QMutex, ksl_mutex)

class MyKeyStoreList;
static MyKeyStoreList *keyStoreList = 0;

static void gpg_keyStoreLog(const QString &str);

class MyPGPKeyContext : public PGPKeyContext
{
public:
	PGPKeyContextProps _props;

	// keys loaded externally (not from the keyring) need to have these
	//   values cached, since we can't extract them later
	QByteArray cacheExportBinary;
	QString cacheExportAscii;

	MyPGPKeyContext(Provider *p) : PGPKeyContext(p)
	{
		// zero out the props
		_props.isSecret = false;
		_props.inKeyring = true;
		_props.isTrusted = false;
	}

	virtual Provider::Context *clone() const
	{
		return new MyPGPKeyContext(*this);
	}

	void set(const GpgOp::Key &i, bool isSecret, bool inKeyring, bool isTrusted)
	{
		const GpgOp::KeyItem &ki = i.keyItems.first();

		_props.keyId = ki.id;
		_props.userIds = i.userIds;
		_props.isSecret = isSecret;
		_props.creationDate = ki.creationDate;
		_props.expirationDate = ki.expirationDate;
		_props.fingerprint = ki.fingerprint.toLower();
		_props.inKeyring = inKeyring;
		_props.isTrusted = isTrusted;
	}

	virtual const PGPKeyContextProps *props() const
	{
		return &_props;
	}

	virtual QByteArray toBinary() const
	{
		if(_props.inKeyring)
		{
			GpgOp gpg(find_bin());
			gpg.setAsciiFormat(false);
			gpg.doExport(_props.keyId);
			gpg_waitForFinished(&gpg);
			gpg_keyStoreLog(gpg.readDiagnosticText());
			if(!gpg.success())
				return QByteArray();
			return gpg.read();
		}
		else
			return cacheExportBinary;
	}

	virtual QString toAscii() const
	{
		if(_props.inKeyring)
		{
			GpgOp gpg(find_bin());
			gpg.setAsciiFormat(true);
			gpg.doExport(_props.keyId);
			gpg_waitForFinished(&gpg);
			gpg_keyStoreLog(gpg.readDiagnosticText());
			if(!gpg.success())
				return QString();
			return QString::fromLocal8Bit(gpg.read());
		}
		else
			return cacheExportAscii;
	}

	static void cleanup_temp_keyring(const QString &name)
	{
		QFile::remove(name);
		QFile::remove(name + '~'); // remove possible backup file
	}

	virtual ConvertResult fromBinary(const QByteArray &a)
	{
		GpgOp::Key key;
		bool sec = false;

		// temporary keyrings
		QString pubname, secname;

		QTemporaryFile pubtmp(QDir::tempPath() + QLatin1String("/qca_gnupg_tmp.XXXXXX.gpg"));
		if(!pubtmp.open())
			return ErrorDecode;

		QTemporaryFile sectmp(QDir::tempPath() + QLatin1String("/qca_gnupg_tmp.XXXXXX.gpg"));
		if(!sectmp.open())
			return ErrorDecode;

		pubname = pubtmp.fileName();
		secname = sectmp.fileName();

		// we turn off autoRemove so that we can close the files
		//   without them getting deleted
		pubtmp.setAutoRemove(false);
		sectmp.setAutoRemove(false);
		pubtmp.close();
		sectmp.close();

		// import key into temporary keyring
		GpgOp gpg(find_bin());
		gpg.setKeyrings(pubname, secname);
		gpg.doImport(a);
		gpg_waitForFinished(&gpg);
		gpg_keyStoreLog(gpg.readDiagnosticText());
		// comment this out.  apparently gpg will report failure for
		//   an import if there are trust issues, even though the
		//   key actually did get imported
		/*if(!gpg.success())
		{
			cleanup_temp_keyring(pubname);
			cleanup_temp_keyring(secname);
			return ErrorDecode;
		}*/

		// now extract the key from gpg like normal

		// is it a public key?
		gpg.doPublicKeys();
		gpg_waitForFinished(&gpg);
		gpg_keyStoreLog(gpg.readDiagnosticText());
		if(!gpg.success())
		{
			cleanup_temp_keyring(pubname);
			cleanup_temp_keyring(secname);
			return ErrorDecode;
		}

		GpgOp::KeyList pubkeys = gpg.keys();
		if(!pubkeys.isEmpty())
		{
			key = pubkeys.first();
		}
		else
		{
			// is it a secret key?
			gpg.doSecretKeys();
			gpg_waitForFinished(&gpg);
			gpg_keyStoreLog(gpg.readDiagnosticText());
			if(!gpg.success())
			{
				cleanup_temp_keyring(pubname);
				cleanup_temp_keyring(secname);
				return ErrorDecode;
			}

			GpgOp::KeyList seckeys = gpg.keys();
			if(!seckeys.isEmpty())
			{
				key = seckeys.first();
				sec = true;
			}
			else
			{
				// no keys found
				cleanup_temp_keyring(pubname);
				cleanup_temp_keyring(secname);
				return ErrorDecode;
			}
		}

		// export binary/ascii and cache

		gpg.setAsciiFormat(false);
		gpg.doExport(key.keyItems.first().id);
		gpg_waitForFinished(&gpg);
		gpg_keyStoreLog(gpg.readDiagnosticText());
		if(!gpg.success())
		{
			cleanup_temp_keyring(pubname);
			cleanup_temp_keyring(secname);
			return ErrorDecode;
		}
		cacheExportBinary = gpg.read();

		gpg.setAsciiFormat(true);
		gpg.doExport(key.keyItems.first().id);
		gpg_waitForFinished(&gpg);
		gpg_keyStoreLog(gpg.readDiagnosticText());
		if(!gpg.success())
		{
			cleanup_temp_keyring(pubname);
			cleanup_temp_keyring(secname);
			return ErrorDecode;
		}
		cacheExportAscii = QString::fromLocal8Bit(gpg.read());

		// all done

		cleanup_temp_keyring(pubname);
		cleanup_temp_keyring(secname);

		set(key, sec, false, false);
		return ConvertGood;
	}

	virtual ConvertResult fromAscii(const QString &s)
	{
		// GnuPG does ascii/binary detection for imports, so for
		//   simplicity we consider an ascii import to just be a
		//   binary import that happens to be comprised of ascii
		return fromBinary(s.toLocal8Bit());
	}
};

class MyKeyStoreEntry : public KeyStoreEntryContext
{
public:
	KeyStoreEntry::Type item_type;
	PGPKey pub, sec;
	QString _storeId, _storeName;

	MyKeyStoreEntry(const PGPKey &_pub, const PGPKey &_sec, Provider *p) : KeyStoreEntryContext(p)
	{
		pub = _pub;
		sec = _sec;
		if(!sec.isNull())
			item_type = KeyStoreEntry::TypePGPSecretKey;
		else
			item_type = KeyStoreEntry::TypePGPPublicKey;
	}

	MyKeyStoreEntry(const MyKeyStoreEntry &from) : KeyStoreEntryContext(from)
	{
	}

	~MyKeyStoreEntry()
	{
	}

	virtual Provider::Context *clone() const
	{
		return new MyKeyStoreEntry(*this);
	}

	virtual KeyStoreEntry::Type type() const
	{
		return item_type;
	}

	virtual QString name() const
	{
		return pub.primaryUserId();
	}

	virtual QString id() const
	{
		return pub.keyId();
	}

	virtual QString storeId() const
	{
		return _storeId;
	}

	virtual QString storeName() const
	{
		return _storeName;
	}

	virtual PGPKey pgpSecretKey() const
	{
		return sec;
	}

	virtual PGPKey pgpPublicKey() const
	{
		return pub;
	}

	virtual QString serialize() const
	{
		// we only serialize the key id.  this means the keyring
		//   must be available to restore the data
		QStringList out;
		out += escape_string("qca-gnupg-1");
		out += escape_string(pub.keyId());
		return out.join(":");
	}
};

// since keyring files are often modified by creating a new copy and
//   overwriting the original file, this messes up Qt's file watching
//   capability since the original file goes away.  to work around this
//   problem, we'll watch the directories containing the keyring files
//   instead of watching the actual files themselves.
//
// FIXME: qca 2.0.1 FileWatch has this logic already, so we can probably
//   simplify this class.
class RingWatch : public QObject
{
	Q_OBJECT
public:
	class DirItem
	{
	public:
		DirWatch *dirWatch;
		SafeTimer *changeTimer;
	};

	class FileItem
	{
	public:
		DirWatch *dirWatch;
		QString fileName;
		bool exists;
		qint64 size;
		QDateTime lastModified;
	};

	QList<DirItem> dirs;
	QList<FileItem> files;

	RingWatch(QObject *parent = 0) :
		QObject(parent)
	{
	}

	~RingWatch()
	{
		clear();
	}

	void add(const QString &filePath)
	{
		QFileInfo fi(filePath);
		QString path = fi.absolutePath();

		// watching this path already?
		DirWatch *dirWatch = 0;
		foreach(const DirItem &di, dirs)
		{
			if(di.dirWatch->dirName() == path)
			{
				dirWatch = di.dirWatch;
				break;
			}
		}

		// if not, make a watcher
		if(!dirWatch)
		{
			//printf("creating dirwatch for [%s]\n", qPrintable(path));

			DirItem di;
			di.dirWatch = new DirWatch(path, this);
			connect(di.dirWatch, SIGNAL(changed()), SLOT(dirChanged()));

			if(qcaVersion() == 0x020000)
				hack_fix(di.dirWatch);

			di.changeTimer = new SafeTimer(this);
			di.changeTimer->setSingleShot(true);
			connect(di.changeTimer, SIGNAL(timeout()), SLOT(handleChanged()));

			dirWatch = di.dirWatch;
			dirs += di;
		}

		FileItem i;
		i.dirWatch = dirWatch;
		i.fileName = fi.fileName();
		i.exists = fi.exists();
		if(i.exists)
		{
			i.size = fi.size();
			i.lastModified = fi.lastModified();
		}
		files += i;

		//printf("watching [%s] in [%s]\n", qPrintable(fi.fileName()), qPrintable(i.dirWatch->dirName()));
	}

	void clear()
	{
		files.clear();

		foreach(const DirItem &di, dirs)
		{
			delete di.changeTimer;
			delete di.dirWatch;
		}

		dirs.clear();
	}

signals:
	void changed(const QString &filePath);

private slots:
	void dirChanged()
	{
		DirWatch *dirWatch = (DirWatch *)sender();

		int at = -1;
		for(int n = 0; n < dirs.count(); ++n)
		{
			if(dirs[n].dirWatch == dirWatch)
			{
				at = n;
				break;
			}
		}
		if(at == -1)
			return;

		// we get a ton of change notifications for the dir when
		//   something happens..   let's collect them and only
		//   report after 100ms

		if(!dirs[at].changeTimer->isActive())
			dirs[at].changeTimer->start(100);
	}

	void handleChanged()
	{
		SafeTimer *t = (SafeTimer *)sender();

		int at = -1;
		for(int n = 0; n < dirs.count(); ++n)
		{
			if(dirs[n].changeTimer == t)
			{
				at = n;
				break;
			}
		}
		if(at == -1)
			return;

		DirWatch *dirWatch = dirs[at].dirWatch;
		QString dir = dirWatch->dirName();

		// see which files changed
		QStringList changeList;
		for(int n = 0; n < files.count(); ++n)
		{
			FileItem &i = files[n];
			QString filePath = dir + '/' + i.fileName;
			QFileInfo fi(filePath);

			// if the file didn't exist, and still doesn't, skip
			if(!i.exists && !fi.exists())
				continue;

			// size/lastModified should only get checked here if
			//   the file existed and still exists
			if(fi.exists() != i.exists || fi.size() != i.size || fi.lastModified() != i.lastModified)
			{
				changeList += filePath;

				i.exists = fi.exists();
				if(i.exists)
				{
					i.size = fi.size();
					i.lastModified = fi.lastModified();
				}
			}
		}

		foreach(const QString &s, changeList)
			emit changed(s);
	}
};

class MyKeyStoreList : public KeyStoreListContext
{
	Q_OBJECT
public:
	int init_step;
	bool initialized;
	GpgOp gpg;
	GpgOp::KeyList pubkeys, seckeys;
	QString pubring, secring;
	bool pubdirty, secdirty;
	RingWatch ringWatch;
	QMutex ringMutex;

	MyKeyStoreList(Provider *p) :
		KeyStoreListContext(p),
		initialized(false),
		gpg(find_bin(), this),
		pubdirty(false),
		secdirty(false),
		ringWatch(this)
	{
		QMutexLocker locker(ksl_mutex());
		keyStoreList = this;

		connect(&gpg, SIGNAL(finished()), SLOT(gpg_finished()));
		connect(&ringWatch, SIGNAL(changed(const QString &)), SLOT(ring_changed(const QString &)));
	}

	~MyKeyStoreList()
	{
		QMutexLocker locker(ksl_mutex());
		keyStoreList = 0;
	}

	virtual Provider::Context *clone() const
	{
		return 0;
	}

	static MyKeyStoreList *instance()
	{
		QMutexLocker locker(ksl_mutex());
		return keyStoreList;
	}

	void ext_keyStoreLog(const QString &str)
	{
		if(str.isEmpty())
			return;

		// FIXME: collect and emit in one pass
		QMetaObject::invokeMethod(this, "diagnosticText", Qt::QueuedConnection, Q_ARG(QString, str));
	}

	virtual void start()
	{
		// kick start our init procedure:
		//   ensure gpg is installed
		//   obtain keyring file names for monitoring
		//   cache initial keyrings

		init_step = 0;
		gpg.doCheck();
	}

	virtual QList<int> keyStores()
	{
		// we just support one fixed keyring, if any
		QList<int> list;
		if(initialized)
			list += 0;
		return list;
	}

	virtual KeyStore::Type type(int) const
	{
		return KeyStore::PGPKeyring;
	}

	virtual QString storeId(int) const
	{
		return "qca-gnupg";
	}

	virtual QString name(int) const
	{
		return "GnuPG Keyring";
	}

	virtual QList<KeyStoreEntry::Type> entryTypes(int) const
	{
		QList<KeyStoreEntry::Type> list;
		list += KeyStoreEntry::TypePGPSecretKey;
		list += KeyStoreEntry::TypePGPPublicKey;
		return list;
	}

	virtual QList<KeyStoreEntryContext*> entryList(int)
	{
		QMutexLocker locker(&ringMutex);

		QList<KeyStoreEntryContext*> out;

		foreach(const GpgOp::Key &pkey, pubkeys)
		{
			PGPKey pub, sec;

			QString id = pkey.keyItems.first().id;

			MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
			// not secret, in keyring
			kc->set(pkey, false, true, pkey.isTrusted);
			pub.change(kc);

			// optional
			sec = getSecKey(id, pkey.userIds);

			MyKeyStoreEntry *c = new MyKeyStoreEntry(pub, sec, provider());
			c->_storeId = storeId(0);
			c->_storeName = name(0);
			out.append(c);
		}

		return out;
	}

	virtual KeyStoreEntryContext *entry(int, const QString &entryId)
	{
		QMutexLocker locker(&ringMutex);

		PGPKey pub = getPubKey(entryId);
		if(pub.isNull())
			return 0;

		// optional
		PGPKey sec = getSecKey(entryId, static_cast<MyPGPKeyContext *>(pub.context())->_props.userIds);

		MyKeyStoreEntry *c = new MyKeyStoreEntry(pub, sec, provider());
		c->_storeId = storeId(0);
		c->_storeName = name(0);
		return c;
	}

	virtual KeyStoreEntryContext *entryPassive(const QString &serialized)
	{
		QMutexLocker locker(&ringMutex);

		QStringList parts = serialized.split(':');
		if(parts.count() < 2)
			return 0;
		if(unescape_string(parts[0]) != "qca-gnupg-1")
			return 0;

		QString entryId = unescape_string(parts[1]);
		if(entryId.isEmpty())
			return 0;

		PGPKey pub = getPubKey(entryId);
		if(pub.isNull())
			return 0;

		// optional
		PGPKey sec = getSecKey(entryId, static_cast<MyPGPKeyContext *>(pub.context())->_props.userIds);

		MyKeyStoreEntry *c = new MyKeyStoreEntry(pub, sec, provider());
		c->_storeId = storeId(0);
		c->_storeName = name(0);
		return c;
	}

	// TODO: cache should reflect this change immediately
	virtual QString writeEntry(int, const PGPKey &key)
	{
		const MyPGPKeyContext *kc = static_cast<const MyPGPKeyContext *>(key.context());
		QByteArray buf = kc->toBinary();

		GpgOp gpg(find_bin());
		gpg.doImport(buf);
		gpg_waitForFinished(&gpg);
		gpg_keyStoreLog(gpg.readDiagnosticText());
		if(!gpg.success())
			return QString();

		return kc->_props.keyId;
	}

	// TODO: cache should reflect this change immediately
	virtual bool removeEntry(int, const QString &entryId)
	{
		ringMutex.lock();
		PGPKey pub = getPubKey(entryId);
		ringMutex.unlock();

		const MyPGPKeyContext *kc = static_cast<const MyPGPKeyContext *>(pub.context());
		QString fingerprint = kc->_props.fingerprint;

		GpgOp gpg(find_bin());
		gpg.doDeleteKey(fingerprint);
		gpg_waitForFinished(&gpg);
		gpg_keyStoreLog(gpg.readDiagnosticText());
		return gpg.success();
	}

	// internal
	PGPKey getPubKey(const QString &keyId) const
	{
		int at = -1;
		for(int n = 0; n < pubkeys.count(); ++n)
		{
			if(pubkeys[n].keyItems.first().id == keyId)
			{
				at = n;
				break;
			}
		}
		if(at == -1)
			return PGPKey();

		const GpgOp::Key &pkey = pubkeys[at];

		PGPKey pub;
		MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
		// not secret, in keyring
		kc->set(pkey, false, true, pkey.isTrusted);
		pub.change(kc);

		return pub;
	}

	// internal
	PGPKey getSecKey(const QString &keyId, const QStringList &userIdsOverride) const
	{
		Q_UNUSED(userIdsOverride);

		int at = -1;
		for(int n = 0; n < seckeys.count(); ++n)
		{
			if(seckeys[n].keyItems.first().id == keyId)
			{
				at = n;
				break;
			}
		}
		if(at == -1)
			return PGPKey();

		const GpgOp::Key &skey = seckeys[at];

		PGPKey sec;
		MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
		// secret, in keyring, trusted
		kc->set(skey, true, true, true);
		//kc->_props.userIds = userIdsOverride;
		sec.change(kc);

		return sec;
	}

	PGPKey publicKeyFromId(const QString &keyId)
	{
		QMutexLocker locker(&ringMutex);

		int at = -1;
		for(int n = 0; n < pubkeys.count(); ++n)
		{
			const GpgOp::Key &pkey = pubkeys[n];
			for(int k = 0; k < pkey.keyItems.count(); ++k)
			{
				const GpgOp::KeyItem &ki = pkey.keyItems[k];
				if(ki.id == keyId)
				{
					at = n;
					break;
				}
			}
			if(at != -1)
				break;
		}
		if(at == -1)
			return PGPKey();

		const GpgOp::Key &pkey = pubkeys[at];

		PGPKey pub;
		MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
		// not secret, in keyring
		kc->set(pkey, false, true, pkey.isTrusted);
		pub.change(kc);

		return pub;
	}

	PGPKey secretKeyFromId(const QString &keyId)
	{
		QMutexLocker locker(&ringMutex);

		int at = -1;
		for(int n = 0; n < seckeys.count(); ++n)
		{
			const GpgOp::Key &skey = seckeys[n];
			for(int k = 0; k < skey.keyItems.count(); ++k)
			{
				const GpgOp::KeyItem &ki = skey.keyItems[k];
				if(ki.id == keyId)
				{
					at = n;
					break;
				}
			}
			if(at != -1)
				break;
		}
		if(at == -1)
			return PGPKey();

		const GpgOp::Key &skey = seckeys[at];

		PGPKey sec;
		MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
		// secret, in keyring, trusted
		kc->set(skey, true, true, true);
		sec.change(kc);

		return sec;
	}

private slots:
	void gpg_finished()
	{
		gpg_keyStoreLog(gpg.readDiagnosticText());

		if(!initialized)
		{
			// any steps that fail during init, just give up completely
			if(!gpg.success())
			{
				ringWatch.clear();
				emit busyEnd();
				return;
			}

			// check
			if(init_step == 0)
			{
				// obtain keyring file names for monitoring
				init_step = 1;
				gpg.doSecretKeyringFile();
			}
			// secret keyring filename
			else if(init_step == 1)
			{
				secring = QFileInfo(gpg.keyringFile()).canonicalFilePath();

				if(qt_buggy_fsw())
					fprintf(stderr, "qca-gnupg: disabling keyring monitoring in Qt version < 4.3.5 or 4.4.1\n");

				if(!secring.isEmpty())
				{
					if(!qt_buggy_fsw())
						ringWatch.add(secring);
				}

				// obtain keyring file names for monitoring
				init_step = 2;
				gpg.doPublicKeyringFile();
			}
			// public keyring filename
			else if(init_step == 2)
			{
				pubring = QFileInfo(gpg.keyringFile()).canonicalFilePath();
				if(!pubring.isEmpty())
				{
					if(!qt_buggy_fsw())
						ringWatch.add(pubring);
				}

				// cache initial keyrings
				init_step = 3;
				gpg.doSecretKeys();
			}
			else if(init_step == 3)
			{
				ringMutex.lock();
				seckeys = gpg.keys();
				ringMutex.unlock();

				// cache initial keyrings
				init_step = 4;
				gpg.doPublicKeys();
			}
			else if(init_step == 4)
			{
				ringMutex.lock();
				pubkeys = gpg.keys();
				ringMutex.unlock();

				initialized = true;
				handleDirtyRings();
				emit busyEnd();
			}
		}
		else
		{
			if(!gpg.success())
				return;

			GpgOp::Type op = gpg.op();
			if(op == GpgOp::SecretKeys)
			{
				ringMutex.lock();
				seckeys = gpg.keys();
				ringMutex.unlock();

				secdirty = false;
			}
			else if(op == GpgOp::PublicKeys)
			{
				ringMutex.lock();
				pubkeys = gpg.keys();
				ringMutex.unlock();

				pubdirty = false;
			}

			if(!secdirty && !pubdirty)
			{
				emit storeUpdated(0);
				return;
			}

			handleDirtyRings();
		}
	}

	void ring_changed(const QString &filePath)
	{
		ext_keyStoreLog(QString("ring_changed: [%1]\n").arg(filePath));

		if(filePath == secring)
			sec_changed();
		else if(filePath == pubring)
			pub_changed();
	}

private:
	void pub_changed()
	{
		pubdirty = true;
		handleDirtyRings();
	}

	void sec_changed()
	{
		secdirty = true;
		handleDirtyRings();
	}

	void handleDirtyRings()
	{
		if(!initialized || gpg.isActive())
			return;

		if(secdirty)
			gpg.doSecretKeys();
		else if(pubdirty)
			gpg.doPublicKeys();
	}
};

static void gpg_keyStoreLog(const QString &str)
{
	MyKeyStoreList *ksl = MyKeyStoreList::instance();
	if(ksl)
		ksl->ext_keyStoreLog(str);
}

static PGPKey publicKeyFromId(const QString &id)
{
	MyKeyStoreList *ksl = MyKeyStoreList::instance();
	if(!ksl)
		return PGPKey();

	return ksl->publicKeyFromId(id);
}

static PGPKey secretKeyFromId(const QString &id)
{
	MyKeyStoreList *ksl = MyKeyStoreList::instance();
	if(!ksl)
		return PGPKey();

	return ksl->secretKeyFromId(id);
}

class MyOpenPGPContext : public SMSContext
{
public:
	MyOpenPGPContext(Provider *p) : SMSContext(p, "openpgp")
	{
		// TODO
	}

	virtual Provider::Context *clone() const
	{
		return 0;
	}

	virtual MessageContext *createMessage();
};

class MyMessageContext : public MessageContext
{
	Q_OBJECT
public:
	MyOpenPGPContext *sms;

	QString signerId;
	QStringList recipIds;
	Operation op;
	SecureMessage::SignMode signMode;
	SecureMessage::Format format;
	QByteArray in, out, sig;
	int wrote;
	bool ok, wasSigned;
	GpgOp::Error op_err;
	SecureMessageSignature signer;
	GpgOp gpg;
	bool _finished;
	QString dtext;

	PasswordAsker asker;
	TokenAsker tokenAsker;

	MyMessageContext(MyOpenPGPContext *_sms, Provider *p) : MessageContext(p, "pgpmsg"), gpg(find_bin())
	{
		sms = _sms;
		wrote = 0;
		ok = false;
		wasSigned = false;

		connect(&gpg, SIGNAL(readyRead()), SLOT(gpg_readyRead()));
		connect(&gpg, SIGNAL(bytesWritten(int)), SLOT(gpg_bytesWritten(int)));
		connect(&gpg, SIGNAL(finished()), SLOT(gpg_finished()));
		connect(&gpg, SIGNAL(needPassphrase(const QString &)), SLOT(gpg_needPassphrase(const QString &)));
		connect(&gpg, SIGNAL(needCard()), SLOT(gpg_needCard()));
		connect(&gpg, SIGNAL(readyReadDiagnosticText()), SLOT(gpg_readyReadDiagnosticText()));

		connect(&asker, SIGNAL(responseReady()), SLOT(asker_responseReady()));
		connect(&tokenAsker, SIGNAL(responseReady()), SLOT(tokenAsker_responseReady()));
	}

	virtual Provider::Context *clone() const
	{
		return 0;
	}

	virtual bool canSignMultiple() const
	{
		return false;
	}

	virtual SecureMessage::Type type() const
	{
		return SecureMessage::OpenPGP;
	}

	virtual void reset()
	{
		wrote = 0;
		ok = false;
		wasSigned = false;
	}

	virtual void setupEncrypt(const SecureMessageKeyList &keys)
	{
		recipIds.clear();
		for(int n = 0; n < keys.count(); ++n)
			recipIds += keys[n].pgpPublicKey().keyId();
	}

	virtual void setupSign(const SecureMessageKeyList &keys, SecureMessage::SignMode m, bool, bool)
	{
		signerId = keys.first().pgpSecretKey().keyId();
		signMode = m;
	}

	virtual void setupVerify(const QByteArray &detachedSig)
	{
		sig = detachedSig;
	}

	virtual void start(SecureMessage::Format f, Operation op)
	{
		_finished = false;
		format = f;
		this->op = op;

		if(getProperty("pgp-always-trust").toBool())
			gpg.setAlwaysTrust(true);

		if(format == SecureMessage::Ascii)
			gpg.setAsciiFormat(true);
		else
			gpg.setAsciiFormat(false);

		if(op == Encrypt)
		{
			gpg.doEncrypt(recipIds);
		}
		else if(op == Decrypt)
		{
			gpg.doDecrypt();
		}
		else if(op == Sign)
		{
			if(signMode == SecureMessage::Message)
			{
				gpg.doSign(signerId);
			}
			else if(signMode == SecureMessage::Clearsign)
			{
				gpg.doSignClearsign(signerId);
			}
			else // SecureMessage::Detached
			{
				gpg.doSignDetached(signerId);
			}
		}
		else if(op == Verify)
		{
			if(!sig.isEmpty())
				gpg.doVerifyDetached(sig);
			else
				gpg.doDecrypt();
		}
		else if(op == SignAndEncrypt)
		{
			gpg.doSignAndEncrypt(signerId, recipIds);
		}
	}

	virtual void update(const QByteArray &in)
	{
		gpg.write(in);
		//this->in.append(in);
	}

	virtual QByteArray read()
	{
		QByteArray a = out;
		out.clear();
		return a;
	}

	virtual int written()
	{
		int x = wrote;
		wrote = 0;
		return x;
	}

	virtual void end()
	{
		gpg.endWrite();
	}

	void seterror()
	{
		gpg.reset();
		_finished = true;
		ok = false;
		op_err = GpgOp::ErrorUnknown;
	}

	void complete()
	{
		_finished = true;

		dtext = gpg.readDiagnosticText();

		ok = gpg.success();
		if(ok)
		{
			if(op == Sign && signMode == SecureMessage::Detached)
				sig = gpg.read();
			else
				out = gpg.read();
		}

		if(ok)
		{
			if(gpg.wasSigned())
			{
				QString signerId = gpg.signerId();
				QDateTime ts = gpg.timestamp();
				GpgOp::VerifyResult vr = gpg.verifyResult();

				SecureMessageSignature::IdentityResult ir;
				Validity v;
				if(vr == GpgOp::VerifyGood)
				{
					ir = SecureMessageSignature::Valid;
					v = ValidityGood;
				}
				else if(vr == GpgOp::VerifyBad)
				{
					ir = SecureMessageSignature::InvalidSignature;
					v = ValidityGood; // good key, bad sig
				}
				else // GpgOp::VerifyNoKey
				{
					ir = SecureMessageSignature::NoKey;
					v = ErrorValidityUnknown;
				}

				SecureMessageKey key;
				PGPKey pub = publicKeyFromId(signerId);
				if(pub.isNull())
				{
					MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
					kc->_props.keyId = signerId;
					pub.change(kc);
				}
				key.setPGPPublicKey(pub);

				signer = SecureMessageSignature(ir, v, key, ts);
				wasSigned = true;
			}
		}
		else
			op_err = gpg.errorCode();
	}

	virtual bool finished() const
	{
		return _finished;
	}

	virtual bool waitForFinished(int msecs)
	{
		// FIXME
		Q_UNUSED(msecs);

		while(1)
		{
			// TODO: handle token prompt events

			GpgOp::Event e = gpg.waitForEvent(-1);
			if(e.type == GpgOp::Event::NeedPassphrase)
			{
				// TODO

				QString keyId;
				PGPKey sec = secretKeyFromId(e.keyId);
				if(!sec.isNull())
					keyId = sec.keyId();
				else
					keyId = e.keyId;
				QStringList out;
				out += escape_string("qca-gnupg-1");
				out += escape_string(keyId);
				QString serialized = out.join(":");

				KeyStoreEntry kse;
				KeyStoreEntryContext *c = keyStoreList->entryPassive(serialized);
				if(c)
					kse.change(c);

				asker.ask(Event::StylePassphrase, KeyStoreInfo(KeyStore::PGPKeyring, keyStoreList->storeId(0), keyStoreList->name(0)), kse, 0);
				asker.waitForResponse();

				if(!asker.accepted())
				{
					seterror();
					return true;
				}

				gpg.submitPassphrase(asker.password());
			}
			else if(e.type == GpgOp::Event::NeedCard)
			{
				tokenAsker.ask(KeyStoreInfo(KeyStore::PGPKeyring, keyStoreList->storeId(0), keyStoreList->name(0)), KeyStoreEntry(), 0);

				if(!tokenAsker.accepted())
				{
					seterror();
					return true;
				}

				gpg.cardOkay();
			}
			else if(e.type == GpgOp::Event::Finished)
				break;
		}

		complete();
		return true;
	}

	virtual bool success() const
	{
		return ok;
	}

	virtual SecureMessage::Error errorCode() const
	{
		SecureMessage::Error e = SecureMessage::ErrorUnknown;
		if(op_err == GpgOp::ErrorProcess)
			e = SecureMessage::ErrorUnknown;
		else if(op_err == GpgOp::ErrorPassphrase)
			e = SecureMessage::ErrorPassphrase;
		else if(op_err == GpgOp::ErrorFormat)
			e = SecureMessage::ErrorFormat;
		else if(op_err == GpgOp::ErrorSignerExpired)
			e = SecureMessage::ErrorSignerExpired;
		else if(op_err == GpgOp::ErrorEncryptExpired)
			e = SecureMessage::ErrorEncryptExpired;
		else if(op_err == GpgOp::ErrorEncryptUntrusted)
			e = SecureMessage::ErrorEncryptUntrusted;
		else if(op_err == GpgOp::ErrorEncryptInvalid)
			e = SecureMessage::ErrorEncryptInvalid;
		else if(op_err == GpgOp::ErrorDecryptNoKey)
			e = SecureMessage::ErrorUnknown;
		else if(op_err == GpgOp::ErrorUnknown)
			e = SecureMessage::ErrorUnknown;
		return e;
	}

	virtual QByteArray signature() const
	{
		return sig;
	}

	virtual QString hashName() const
	{
		// TODO
		return "sha1";
	}

	virtual SecureMessageSignatureList signers() const
	{
		SecureMessageSignatureList list;
		if(ok && wasSigned)
			list += signer;
		return list;
	}

	virtual QString diagnosticText() const
	{
		return dtext;
	}

private slots:
	void gpg_readyRead()
	{
		emit updated();
	}

	void gpg_bytesWritten(int bytes)
	{
		wrote += bytes;
	}

	void gpg_finished()
	{
		complete();
		emit updated();
	}

	void gpg_needPassphrase(const QString &in_keyId)
	{
		// FIXME: copied from above, clean up later

		QString keyId;
		PGPKey sec = secretKeyFromId(in_keyId);
		if(!sec.isNull())
			keyId = sec.keyId();
		else
			keyId = in_keyId;
		//emit keyStoreList->storeNeedPassphrase(0, 0, keyId);
		QStringList out;
		out += escape_string("qca-gnupg-1");
		out += escape_string(keyId);
		QString serialized = out.join(":");

		KeyStoreEntry kse;
		KeyStoreEntryContext *c = keyStoreList->entryPassive(serialized);
		if(c)
			kse.change(c);

		asker.ask(Event::StylePassphrase, KeyStoreInfo(KeyStore::PGPKeyring, keyStoreList->storeId(0), keyStoreList->name(0)), kse, 0);
	}

	void gpg_needCard()
	{
		tokenAsker.ask(KeyStoreInfo(KeyStore::PGPKeyring, keyStoreList->storeId(0), keyStoreList->name(0)), KeyStoreEntry(), 0);
	}

	void gpg_readyReadDiagnosticText()
	{
		// TODO ?
	}

	void asker_responseReady()
	{
		if(!asker.accepted())
		{
			seterror();
			emit updated();
			return;
		}

		SecureArray a = asker.password();
		gpg.submitPassphrase(a);
	}

	void tokenAsker_responseReady()
	{
		if(!tokenAsker.accepted())
		{
			seterror();
			emit updated();
			return;
		}

		gpg.cardOkay();
	}
};

MessageContext *MyOpenPGPContext::createMessage()
{
	return new MyMessageContext(this, provider());
}

}

using namespace gpgQCAPlugin;

class gnupgProvider : public QCA::Provider
{
public:
	virtual void init()
	{
	}

	virtual int qcaVersion() const
	{
		return QCA_VERSION;
	}

	virtual QString name() const
	{
		return "qca-gnupg";
	}

	virtual QStringList features() const
	{
		QStringList list;
		list += "pgpkey";
		list += "openpgp";
		list += "keystorelist";
		return list;
	}

	virtual Context *createContext(const QString &type)
	{
		if(type == "pgpkey")
			return new MyPGPKeyContext(this);
		else if(type == "openpgp")
			return new MyOpenPGPContext(this);
		else if(type == "keystorelist")
			return new MyKeyStoreList(this);
		else
			return 0;
	}
};

class gnupgPlugin : public QObject, public QCAPlugin
{
	Q_OBJECT
	Q_INTERFACES(QCAPlugin)
public:
	virtual QCA::Provider *createProvider() { return new gnupgProvider; }
};

#include "qca-gnupg.moc"

Q_EXPORT_PLUGIN2(qca_gnupg, gnupgPlugin)
