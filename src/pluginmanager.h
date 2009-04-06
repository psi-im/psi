/*
 * (c) 2006 Kevin Smith
 * (c) 2008 Maciej Niedzielski
 */

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QtCore>
#include <QList>
#include <QMap>
#include <QHash>
#include <QDomElement>

class QPluginLoader;

class PsiAccount;
class PsiPlugin;
class PluginHost;

namespace XMPP {
	class Client;
}

namespace QCA {
	class DirWatch;
}


class PluginManager : public QObject
{
	Q_OBJECT;

public:
	static PluginManager* instance();

	QStringList availablePlugins();

	void addAccount(const PsiAccount* account, XMPP::Client* client);

	void loadEnabledPlugins();
	bool unloadAllPlugins();

	QString pathToPlugin(const QString& plugin);
	QString shortName(const QString& plugin);
	QWidget* optionsWidget(const QString& plugin);

	bool processEvent(const PsiAccount* account, QDomElement& eventXml);
	bool processMessage(const PsiAccount* account, const QString& jidFrom, const QString& body, const QString& subject);
	
	static const QString loadOptionPrefix;
	static const QString pluginOptionPrefix;
	
private:
	PluginManager();
	void loadAllPlugins();
	bool verifyStanza(const QString& stanza);
	void updatePluginsList();

	static PluginManager* instance_;

	//account id, client
	QVector<XMPP::Client*> clients_;

	//account, account id
	QHash<const PsiAccount*, int> accountIds_;

	//name, host
	QHash<QString, PluginHost*> hosts_;
	//file, host
	QHash<QString, PluginHost*> pluginByFile_;

	
	QList<QCA::DirWatch*> dirWatchers_;

	class StreamWatcher;
	bool incomingXml(int account, const QDomElement &eventXml);
	void sendXml(int account, const QString& xml);
	QString uniqueId(int account);

	friend class PluginHost;
	
private slots:
	void dirsChanged();
	void optionChanged(const QString& option);
};

#endif
