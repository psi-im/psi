/*
 * (c) 2006 Kevin Smith
 * (c) 2008 Maciej Niedzielski
 */

#ifndef PLUGINHOST_H
#define PLUGINHOST_H

#include <QDomElement>
#include <QVariant>
#include <QRegExp>
#include <QMultiMap>

#include "stanzasendinghost.h"
#include "iqfilteringhost.h"
#include "optionaccessinghost.h"

class QWidget;
class QPluginLoader;

class PluginManager;
class IqNamespaceFilter;

class PluginHost: public QObject, public StanzaSendingHost, public IqFilteringHost, public OptionAccessingHost
{
	Q_OBJECT;
	Q_INTERFACES(StanzaSendingHost IqFilteringHost OptionAccessingHost);

public:
	PluginHost(PluginManager* manager, const QString& pluginFile);
	virtual ~PluginHost();

	bool isValid() const;
	const QString& path() const;
	QWidget* optionsWidget() const;

	// cached basic info
	const QString& name() const;
	const QString& shortName() const;
	const QString& version() const;

	// loading
	bool load();
	bool unload();
	bool isLoaded() const;

	// enabling
	bool enable();
	bool disable();
	bool isEnabled() const;

	// for StanzaFilter and IqNamespaceFilter
	bool incomingXml(int account, const QDomElement& e);

	// for EventFilter
	bool processEvent(int account, const QDomElement& e);
	bool processMessage(int account, const QString& jidFrom, const QString& body, const QString& subject);

	// StanzaSendingHost
	void sendStanza(int account, const QDomElement& xml);
	void sendStanza(int account, const QString& xml);
	void sendMessage(int account, const QString& to, const QString& body, const QString& subject, const QString& type);
	QString uniqueId(int account);

	// IqFilteringHost
	void addIqNamespaceFilter(const QString& ns, IqNamespaceFilter* filter);
	void addIqNamespaceFilter(const QRegExp& ns, IqNamespaceFilter* filter);
	void removeIqNamespaceFilter(const QString& ns, IqNamespaceFilter* filter);
	void removeIqNamespaceFilter(const QRegExp& ns, IqNamespaceFilter* filter);

	// OptionAccessingHost
	void setPluginOption(const QString& option, const QVariant& value);
	QVariant getPluginOption(const QString& option);
	void setGlobalOption(const QString& option, const QVariant& value);
	QVariant getGlobalOption(const QString& option);

private:
	PluginManager* manager_;
	QObject* plugin_;
	QString file_;
	QString name_;
	QString shortName_;
	QString version_;
	QPluginLoader* loader_;

	bool valid_;
	bool connected_;
	bool enabled_;

	QMultiMap<QString, IqNamespaceFilter*> iqNsFilters_;
	QMultiMap<QRegExp, IqNamespaceFilter*> iqNsxFilters_;

	bool loadPlugin(QObject* pluginObject);

	// disable copying
	PluginHost(const PluginHost&);
	PluginHost& operator=(const PluginHost&);
};


#endif
