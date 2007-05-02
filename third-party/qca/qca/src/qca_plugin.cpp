/*
 * Copyright (C) 2004-2007  Justin Karneges  <justin@affinix.com>
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

// Note: The basic thread-safety approach with the plugin manager is that
//   it is safe to add/get providers, however it is unsafe to remove them.
//   The expectation is that providers will almost always be unloaded on
//   application shutdown.  For safe provider unload, ensure no threads are
//   using the manager, the provider in question, nor any sub-objects from
//   the provider.

#include "qca_plugin.h"

#include "qcaprovider.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QPluginLoader>

#define PLUGIN_SUBDIR "crypto"

namespace QCA {

// from qca_core.cpp
QVariantMap getProviderConfig_internal(Provider *p);

// from qca_default.cpp
QStringList skip_plugins(Provider *defaultProvider);

static ProviderManager *g_pluginman = 0;

static void logDebug(const QString &str)
{
	if(g_pluginman)
		g_pluginman->appendDiagnosticText(str + '\n');
}

static bool validVersion(int ver)
{
	// make sure the provider isn't newer than qca
	if((ver & 0xffff00) <= (QCA_VERSION & 0xffff00))
		return true;
	return false;
}

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
		logDebug(QString("PluginInstance fromFile [%1]").arg(fname));
		QPluginLoader *loader = new QPluginLoader(fname);
		if(!loader->load())
		{
			logDebug("failed to load");
			delete loader;
			return 0;
		}
		QObject *obj = loader->instance();
		if(!obj)
		{
			logDebug("failed to get instance");
			loader->unload();
			delete loader;
			return 0;
		}
		PluginInstance *i = new PluginInstance;
		i->_loader = loader;
		i->_instance = obj;
		i->_ownInstance = true;
		logDebug(QString("loaded as [%1]").arg(obj->metaObject()->className()));
		return i;
	}

	static PluginInstance *fromStatic(QObject *obj)
	{
		logDebug("PluginInstance fromStatic");
		PluginInstance *i = new PluginInstance;
		i->_loader = 0;
		i->_instance = obj;
		i->_ownInstance = false;
		logDebug(QString("loaded as [%1]").arg(obj->metaObject()->className()));
		return i;
	}

	static PluginInstance *fromInstance(QObject *obj)
	{
		logDebug("PluginInstance fromInstance");
		PluginInstance *i = new PluginInstance;
		i->_loader = 0;
		i->_instance = obj;
		i->_ownInstance = true;
		logDebug(QString("loaded as [%1]").arg(obj->metaObject()->className()));
		return i;
	}

	~PluginInstance()
	{
		QString str;
		if(_instance)
			str = _instance->metaObject()->className();

		if(_ownInstance)
			delete _instance;

		if(_loader)
		{
			_loader->unload();
			delete _loader;
		}
		logDebug(QString("PluginInstance deleted [%1]").arg(str));
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
};

class ProviderItem
{
public:
	QString fname;
	Provider *p;
	int priority;
	QMutex m;

	static ProviderItem *load(const QString &fname)
	{
		PluginInstance *i = PluginInstance::fromFile(fname);
		if(!i)
			return 0;
		QCAPlugin *plugin = qobject_cast<QCAPlugin*>(i->instance());
		if(!plugin)
		{
			logDebug("not a QCAPlugin or wrong interface");
			delete i;
			return 0;
		}

		Provider *p = plugin->createProvider();
		if(!p)
		{
			logDebug("unable to create provider");
			delete i;
			return 0;
		}

		ProviderItem *pi = new ProviderItem(i, p);
		pi->fname = fname;
		return pi;
	}

	static ProviderItem *loadStatic(QObject *instance)
	{
		PluginInstance *i = PluginInstance::fromStatic(instance);
		QCAPlugin *plugin = qobject_cast<QCAPlugin*>(i->instance());
		if(!plugin)
		{
			logDebug("not a QCAPlugin or wrong interface");
			delete i;
			return 0;
		}

		Provider *p = plugin->createProvider();
		if(!p)
		{
			logDebug("unable to create provider");
			delete i;
			return 0;
		}

		ProviderItem *pi = new ProviderItem(i, p);
		return pi;
	}

	static ProviderItem *fromClass(Provider *p)
	{
		ProviderItem *pi = new ProviderItem(0, p);
		return pi;
	}

	~ProviderItem()
	{
		delete p;
		delete instance;
	}

	void ensureInit()
	{
		QMutexLocker locker(&m);
		if(init_done)
			return;
		init_done = true;

		p->init();

		// load config
		QVariantMap conf = getProviderConfig_internal(p);
		if(!conf.isEmpty())
			p->configChanged(conf);
	}

private:
	PluginInstance *instance;
	bool init_done;

	ProviderItem(PluginInstance *_instance, Provider *_p)
	{
		instance = _instance;
		p = _p;
		init_done = false;

		// disassociate from threads
		if(instance)
			instance->claim();
		logDebug(QString("ProviderItem created: [%1]").arg(p->name()));
	}
};

ProviderManager::ProviderManager()
{
	g_pluginman = this;
	def = 0;
	scanned_static = false;
}

ProviderManager::~ProviderManager()
{
	unloadAll();
	delete def;
	g_pluginman = 0;
}

void ProviderManager::scan()
{
	QMutexLocker locker(&providerMutex);

	// check static first, but only once
	if(!scanned_static)
	{
		QObjectList list = QPluginLoader::staticInstances();
		for(int n = 0; n < list.count(); ++n)
		{
			QObject *instance = list[n];
			ProviderItem *i = ProviderItem::loadStatic(instance);
			if(!i)
				continue;

			if(haveAlready(i->p->name()))
			{
				logDebug("skipping, we already have it");
				delete i;
				continue;
			}

			int ver = i->p->version();
			if(!validVersion(ver))
			{
				logDebug(QString().sprintf("plugin version 0x%06x is in the future", ver));
				delete i;
				continue;
			}

			addItem(i, -1);
		}
		scanned_static = true;
	}

	if(qgetenv("QCA_NO_PLUGINS") == "1")
		return;

	// check plugin files
	QStringList dirs = QCoreApplication::libraryPaths();
	if(dirs.isEmpty())
		logDebug("no Qt plugin paths");
	for(QStringList::ConstIterator it = dirs.begin(); it != dirs.end(); ++it)
	{
		logDebug(QString("checking in path: [%1]").arg(*it));
		QDir libpath(*it);
		QDir dir(libpath.filePath(PLUGIN_SUBDIR));
		if(!dir.exists())
			continue;

		foreach(const QString maybeFile, dir.entryList())
		{
			QFileInfo fi(dir.filePath(maybeFile));
			if(fi.isDir())
				continue;
			QString fname = fi.filePath();

			logDebug(QString("checking file: [%1]").arg(fname));
			if(!QLibrary::isLibrary(fname))
			{
				logDebug("skipping, not a library\n");
				continue;
			}

			// make sure we haven't loaded this file before
			bool haveFile = false;
			for(int n = 0; n < providerItemList.count(); ++n)
			{
				ProviderItem *pi = providerItemList[n];
				if(!pi->fname.isEmpty() && pi->fname == fname)
				{
					haveFile = true;
					break;
				}
			}
			if(haveFile)
			{
				logDebug("skipping, we already loaded this file");
				continue;
			}

			ProviderItem *i = ProviderItem::load(fname);
			if(!i)
				continue;

			if(haveAlready(i->p->name()))
			{
				logDebug("skipping, we already have it");
				delete i;
				continue;
			}

			int ver = i->p->version();
			if(!validVersion(ver))
			{
				logDebug(QString().sprintf("plugin version 0x%06x is in the future", ver));
				delete i;
				continue;
			}

			if(skip_plugins(def).contains(i->p->name()))
			{
				logDebug("skipping, explicitly disabled");
				delete i;
				continue;
			}

			addItem(i, -1);
		}
	}
}

bool ProviderManager::add(Provider *p, int priority)
{
	QMutexLocker locker(&providerMutex);

	logDebug(QString("adding pre-made provider: [%1]").arg(p->name()));
	if(haveAlready(p->name()))
	{
		logDebug("skipping, we already have it");
		return false;
	}

	int ver = p->version();
	if(!validVersion(ver))
	{
		logDebug(QString().sprintf("plugin version 0x%06x is in the future", ver));
		return false;
	}

	ProviderItem *i = ProviderItem::fromClass(p);
	addItem(i, priority);
	return true;
}

void ProviderManager::unload(const QString &name)
{
	for(int n = 0; n < providerItemList.count(); ++n)
	{
		ProviderItem *i = providerItemList[n];
		if(i->p && i->p->name() == name)
		{
			delete i;
			providerItemList.removeAt(n);
			providerList.removeAt(n);
			return;
		}
	}
}

void ProviderManager::unloadAll()
{
	qDeleteAll(providerItemList);
	providerItemList.clear();
	providerList.clear();
}

void ProviderManager::setDefault(Provider *p)
{
	QMutexLocker locker(&providerMutex);

	if(def)
		delete def;
	def = p;
	if(def)
	{
		def->init();
		QVariantMap conf = getProviderConfig_internal(def);
		if(!conf.isEmpty())
			def->configChanged(conf);
	}
}

Provider *ProviderManager::find(Provider *_p) const
{
	ProviderItem *i = 0;
	Provider *p = 0;

	providerMutex.lock();
	if(_p == def)
	{
		p = def;
	}
	else
	{
		for(int n = 0; n < providerItemList.count(); ++n)
		{
			ProviderItem *pi = providerItemList[n];
			if(pi->p && pi->p == _p)
			{
				i = pi;
				p = pi->p;
				break;
			}
		}
	}
	providerMutex.unlock();

	if(i)
		i->ensureInit();
	return p;
}

Provider *ProviderManager::find(const QString &name) const
{
	ProviderItem *i = 0;
	Provider *p = 0;

	providerMutex.lock();
	if(def && name == def->name())
	{
		p = def;
	}
	else
	{
		for(int n = 0; n < providerItemList.count(); ++n)
		{
			ProviderItem *pi = providerItemList[n];
			if(pi->p && pi->p->name() == name)
			{
				i = pi;
				p = pi->p;
				break;
			}
		}
	}
	providerMutex.unlock();

	if(i)
		i->ensureInit();
	return p;
}

Provider *ProviderManager::findFor(const QString &name, const QString &type) const
{
	if(name.isEmpty())
	{
		providerMutex.lock();
		QList<ProviderItem*> list = providerItemList;
		providerMutex.unlock();

		// find the first one that can do it
		for(int n = 0; n < list.count(); ++n)
		{
			ProviderItem *pi = list[n];
			pi->ensureInit();
			if(pi->p && pi->p->features().contains(type))
				return pi->p;
		}

		// try the default provider as a last resort
		providerMutex.lock();
		Provider *p = def;
		providerMutex.unlock();
		if(p && p->features().contains(type))
			return p;

		return 0;
	}
	else
	{
		Provider *p = find(name);
		if(p && p->features().contains(type))
			return p;
		return 0;
	}
}

void ProviderManager::changePriority(const QString &name, int priority)
{
	QMutexLocker locker(&providerMutex);

	ProviderItem *i = 0;
	int n = 0;
	for(; n < providerItemList.count(); ++n)
	{
		ProviderItem *pi = providerItemList[n];
		if(pi->p && pi->p->name() == name)
		{
			i = pi;
			break;
		}
	}
	if(!i)
		return;

	providerItemList.removeAt(n);
	providerList.removeAt(n);

	addItem(i, priority);
}

int ProviderManager::getPriority(const QString &name)
{
	QMutexLocker locker(&providerMutex);

	ProviderItem *i = 0;
	for(int n = 0; n < providerItemList.count(); ++n)
	{
		ProviderItem *pi = providerItemList[n];
		if(pi->p && pi->p->name() == name)
		{
			i = pi;
			break;
		}
	}
	if(!i)
		return -1;

	return i->priority;
}

QStringList ProviderManager::allFeatures() const
{
	QStringList featureList;

	providerMutex.lock();
	Provider *p = def;
	providerMutex.unlock();
	if(p)
		featureList = p->features();

	providerMutex.lock();
	QList<ProviderItem*> list = providerItemList;
	providerMutex.unlock();
	for(int n = 0; n < list.count(); ++n)
	{
		ProviderItem *i = list[n];
		if(i->p)
			mergeFeatures(&featureList, i->p->features());
	}

	return featureList;
}

ProviderList ProviderManager::providers() const
{
	QMutexLocker locker(&providerMutex);

	return providerList;
}

QString ProviderManager::diagnosticText() const
{
	QMutexLocker locker(&logMutex);

	return dtext;
}

void ProviderManager::appendDiagnosticText(const QString &str)
{
	QMutexLocker locker(&logMutex);

	dtext += str;
}

void ProviderManager::clearDiagnosticText()
{
	QMutexLocker locker(&logMutex);

	dtext = QString();
}

void ProviderManager::addItem(ProviderItem *item, int priority)
{
	if(priority < 0)
	{
		// for -1, make the priority the same as the last item
		if(!providerItemList.isEmpty())
		{
			ProviderItem *last = providerItemList.last();
			item->priority = last->priority;
		}
		else
			item->priority = 0;

		providerItemList.append(item);
		providerList.append(item->p);
	}
	else
	{
		// place the item before any other items with same or greater priority
		int n = 0;
		for(; n < providerItemList.count(); ++n)
		{
			ProviderItem *i = providerItemList[n];
			if(i->priority >= priority)
				break;
		}

		item->priority = priority;
		providerItemList.insert(n, item);
		providerList.insert(n, item->p);
	}

	logDebug(QString("item added [%1]").arg(item->p->name()));
}

bool ProviderManager::haveAlready(const QString &name) const
{
	if(def && name == def->name())
		return true;

	for(int n = 0; n < providerItemList.count(); ++n)
	{
		ProviderItem *pi = providerItemList[n];
		if(pi->p && pi->p->name() == name)
			return true;
	}

	return false;
}

void ProviderManager::mergeFeatures(QStringList *a, const QStringList &b)
{
	for(QStringList::ConstIterator it = b.begin(); it != b.end(); ++it)
	{
		if(!a->contains(*it))
			a->append(*it);
	}
}

}
