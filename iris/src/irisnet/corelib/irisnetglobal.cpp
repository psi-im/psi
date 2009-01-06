/*
 * Copyright (C) 2006  Justin Karneges
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

#include "irisnetglobal_p.h"

#include "irisnetplugin.h"

namespace XMPP {

// built-in providers
#ifdef Q_OS_WIN
extern IrisNetProvider *irisnet_createWinNetProvider();
#endif
#ifdef Q_OS_UNIX
extern IrisNetProvider *irisnet_createUnixNetProvider();
#endif
extern IrisNetProvider *irisnet_createJDnsProvider();
#ifdef APPLEDNS_STATIC
extern IrisNetProvider *irisnet_createAppleProvider();
#endif

//----------------------------------------------------------------------------
// internal
//----------------------------------------------------------------------------
class PluginInstance
{
private:
	QPluginLoader *_loader;
	QObject *_instance;
	bool _ownInstance;

	PluginInstance()
	{
	}

public:
	static PluginInstance *fromFile(const QString &fname)
	{
		QPluginLoader *loader = new QPluginLoader(fname);
		if(!loader->load())
		{
			delete loader;
			return 0;
		}
		QObject *obj = loader->instance();
		if(!obj)
		{
			loader->unload();
			delete loader;
			return 0;
		}
		PluginInstance *i = new PluginInstance;
		i->_loader = loader;
		i->_instance = obj;
		i->_ownInstance = true;
		return i;
	}

	static PluginInstance *fromStatic(QObject *obj)
	{
		PluginInstance *i = new PluginInstance;
		i->_loader = 0;
		i->_instance = obj;
		i->_ownInstance = false;
		return i;
	}

	static PluginInstance *fromInstance(QObject *obj)
	{
		PluginInstance *i = new PluginInstance;
		i->_loader = 0;
		i->_instance = obj;
		i->_ownInstance = true;
		return i;
	}

	~PluginInstance()
	{
		if(_ownInstance)
			delete _instance;

		if(_loader)
		{
			_loader->unload();
			delete _loader;
		}
	}

	void claim()
	{
		if(_loader)
			_loader->moveToThread(0);
		if(_ownInstance)
			_instance->moveToThread(0);
	}

	QObject *instance()
	{
		return _instance;
	}

	bool sameType(const PluginInstance *other)
	{
		if(!_instance || !other->_instance)
			return false;

		if(qstrcmp(_instance->metaObject()->className(), other->_instance->metaObject()->className()) != 0)
			return false;

		return true;
	}
};

class PluginManager
{
public:
	bool builtin_done;
	QStringList paths;
	QList<PluginInstance*> plugins;
	QList<IrisNetProvider*> providers;

	PluginManager()
	{
		builtin_done = false;
	}

	~PluginManager()
	{
		unload();
	}

	bool tryAdd(PluginInstance *i, bool lowPriority = false)
	{
		// is it the right kind of plugin?
		IrisNetProvider *p = qobject_cast<IrisNetProvider*>(i->instance());
		if(!p)
			return false;

		// make sure we don't have it already
		for(int n = 0; n < plugins.count(); ++n)
		{
			if(i->sameType(plugins[n]))
				return false;
		}

		i->claim();
		plugins += i;
		if(lowPriority)
			providers.append(p);
		else
			providers.prepend(p);
		return true;
	}

	void addBuiltIn(IrisNetProvider *p)
	{
		PluginInstance *i = PluginInstance::fromInstance(p);
		if(!tryAdd(i, true))
			delete i;
	}

	void scan()
	{
		if(!builtin_done)
		{
#ifdef Q_OS_WIN
			addBuiltIn(irisnet_createWinNetProvider());
#endif
#ifdef Q_OS_UNIX
			addBuiltIn(irisnet_createUnixNetProvider());
#endif
			addBuiltIn(irisnet_createJDnsProvider());
			builtin_done = true;
		}

		QObjectList list = QPluginLoader::staticInstances();
		for(int n = 0; n < list.count(); ++n)
		{
			PluginInstance *i = PluginInstance::fromStatic(list[n]);
			if(!tryAdd(i))
				delete i;
		}
		for(int n = 0; n < paths.count(); ++n)
		{
			QDir dir(paths[n]);
			if(!dir.exists())
				continue;

			QStringList entries = dir.entryList();
			for(int k = 0; k < entries.count(); ++k)
			{
				QFileInfo fi(dir.filePath(entries[k]));
				if(!fi.exists())
					continue;
				QString fname = fi.filePath();
				PluginInstance *i = PluginInstance::fromFile(fname);
				if(!i)
					continue;

				if(!tryAdd(i))
					delete i;
			}
		}
	}

	void unload()
	{
		// unload in reverse order
		QList<PluginInstance*> revlist;
		for(int n = 0; n < plugins.count(); ++n)
			revlist.prepend(plugins[n]);
		qDeleteAll(revlist);

		plugins.clear();
		providers.clear();
	}
};

class IrisNetGlobal
{
public:
	QMutex m;
	PluginManager pluginManager;
	QList<IrisNetCleanUpFunction> cleanupList;
};

Q_GLOBAL_STATIC(QMutex, global_mutex)
static IrisNetGlobal *global = 0;

static void deinit();

static void init()
{
	QMutexLocker locker(global_mutex());
	if(global)
		return;

	global = new IrisNetGlobal;
	qAddPostRoutine(deinit);
}

void deinit()
{
	if(!global)
		return;

	while(!global->cleanupList.isEmpty())
		(global->cleanupList.takeFirst())();

	delete global;
	global = 0;
}

//----------------------------------------------------------------------------
// Global
//----------------------------------------------------------------------------
void irisNetSetPluginPaths(const QStringList &paths)
{
	init();

	QMutexLocker locker(&global->m);
	global->pluginManager.paths = paths;
}

void irisNetCleanup()
{
	deinit();
}

void irisNetAddPostRoutine(IrisNetCleanUpFunction func)
{
	init();

	QMutexLocker locker(&global->m);
	global->cleanupList.prepend(func);
}

QList<IrisNetProvider*> irisNetProviders()
{
	init();

	QMutexLocker locker(&global->m);
	global->pluginManager.scan();
	return global->pluginManager.providers;
}

}
