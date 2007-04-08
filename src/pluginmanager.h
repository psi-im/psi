#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QtCore>
#include <QList>
#include <QMap>

#include "userlist.h"
#include "optionstree.h"

class PsiPlugin;

class PsiAccount;
class UserLisItem;
namespace XMPP {
	class Jid;
}
namespace QCA {
	class DirWatch;
}

class QPluginLoader;

class PluginManager : public QObject
{
	Q_OBJECT
public:
	static PluginManager* instance();

	QStringList availablePlugins();

	void addAccount( const PsiAccount* account, XMPP::Client* client);
	void message( PsiAccount* account, const XMPP::Jid& from, 
	const UserListItem*, const QString& message );
	bool loadPlugin( const QString& file );
	void loadEnabledPlugins();
	bool unloadPlugin( const QString& file );
	bool unloadAllPlugins();
	QString pathToPlugin( const QString& plugin );
	QString shortName( const QString& plugin );
	QWidget* getOptionsWidget( const QString& plugin );
	bool processEvent( const PsiAccount* account, QDomElement &event );
	
	const QVariant getGlobalOption(const QString& option);
	
	static const QString loadOptionPrefix;
	static const QString pluginOptionPrefix;
	
protected:
	bool loadPlugin( QObject* pluginObject );

private:
	PluginManager();
	void loadAllPlugins();
	bool verifyStanza(const QString& stanza);
	
	static PluginManager* instance_;
	
	//name, plugin
	QMap<QString, PsiPlugin*> plugins_;
	//name, shortName
	QMap<QString, QString> shortNames_;
	//name, file
	QMap<QString, QString> files_;
	//filename, loader
	QMap<QString, QPluginLoader*> loaders_;
	//account, client
	QMap<const PsiAccount*, XMPP::Client*> clients_;
	
	QList<QCA::DirWatch*> dirWatchers_;
	OptionsTree options_;
	
private slots:
	void dirsChanged();
	void setPluginOption( const QString&, const QVariant& );
	void getPluginOption( const QString&, QVariant&);
	void setGlobalOption( const QString&, const QVariant& );
	void getGlobalOption( const QString&, QVariant&);
	void optionChanged(const QString& option);
	void sendStanza( const PsiAccount* account, const QDomElement& stanza);
	void sendStanza( const PsiAccount* account, const QString& stanza);
};

#endif
