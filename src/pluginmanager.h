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
#include <QMenu>

class QPluginLoader;

class PsiAccount;
class PsiPlugin;
class PluginHost;
class PsiCon;

#define PLUGINS_NO_DEBUG

namespace XMPP {
	class Client;
}

namespace QCA {
	class DirWatch;
}

class AccountIds
{
public:
	int appendAccount(PsiAccount *acc);
	void removeAccount(PsiAccount *acc);
	void clear();
	bool isValidRange(int id) const { return id_keys.contains(id); }
	PsiAccount *account(int id) const;
	int id(PsiAccount *acc) const;

private:
	QHash<PsiAccount *, int> acc_keys;
	QHash<int, PsiAccount *> id_keys;
};

class PluginManager : public QObject
{
	Q_OBJECT

public:
	static PluginManager* instance();
	void initNewSession(PsiCon *psi);

	QStringList availablePlugins() const;

	void addAccount(PsiAccount* account, XMPP::Client* client);

	void loadEnabledPlugins();
	bool unloadAllPlugins();

	bool isEnabled(const QString& plugin) const;
	QString pathToPlugin(const QString& plugin) const;
	QString shortName(const QString& plugin) const;
	QString nameByShortName(const QString& shortName) const;
	QString version(const QString& plugin) const;
	QWidget* optionsWidget(const QString& plugin);

	void setShortcuts();

	bool processEvent(PsiAccount* account, QDomElement& eventXml);
	bool processMessage(PsiAccount* account, const QString& jidFrom, const QString& body, const QString& subject);
	bool processOutgoingMessage(PsiAccount* account, const QString& jidTo, QString& body, const QString& type, QString& subject);
	void processOutgoingStanza(PsiAccount* account, QDomElement &stanza);
	void logout(PsiAccount* account);

	void applyOptions(const QString& plugin);
	void restoreOptions(const QString& plugin);
	void addToolBarButton(QObject* parent, QWidget* toolbar, PsiAccount* account, const QString& contact, const QString& plugin = "");
	bool hasToolBarButton(const QString& plugin) const;
	void addGCToolBarButton(QObject* parent, QWidget* toolbar, PsiAccount* account, const QString& contact, const QString& plugin = "");
	bool hasGCToolBarButton(const QString& plugin) const;
	void addAccountMenu(QMenu *menu, PsiAccount* account);
	void addContactMenu(QMenu *menu, PsiAccount* account, QString jid);

	void setupChatTab(QWidget *tab, PsiAccount* account, const QString& contact);
	void setupGCTab(QWidget *tab, PsiAccount* account, const QString& contact);
	bool appendingChatMessage(PsiAccount* account, const QString& contact,
				  QString& body, QDomElement& html, bool local);

	QString pluginInfo(const QString& plugin) const;
	bool hasInfoProvider(const QString& plugin) const;
	QIcon icon(const QString& plugin) const;

	static const QString loadOptionPrefix;
	static const QString pluginOptionPrefix;

private:
	PluginManager();
	PsiCon *psi_;
	void loadAllPlugins();
	bool verifyStanza(const QString& stanza);
	QList<PluginHost*> updatePluginsList();
	void loadPluginIfEnabled(PluginHost* plugin);

	static PluginManager* instance_;

	//account id, client
	QVector<XMPP::Client*> clients_;

	//account, account id
	AccountIds accountIds_;

	//name, host
	QMap<QString, PluginHost*> hosts_;
	//file, host
	QMap<QString, PluginHost*> pluginByFile_;
	//sorted by priority
	QList<PluginHost*> pluginsByPriority_;


	QList<QCA::DirWatch*> dirWatchers_;

	// Options widget provides by plugin on opt_plugins
	QPointer<QWidget> optionsWidget_;

	class StreamWatcher;
	bool incomingXml(int account, const QDomElement &eventXml);
	void sendXml(int account, const QString& xml);
	QString uniqueId(int account) const;

	QString getStatus(int account) const;
	QString getStatusMessage(int account) const;
	QString proxyHost(int account) const;
	int proxyPort(int account) const;
	QString proxyUser(int account) const;
	QString proxyPassword(int account) const;
	QStringList getRoster(int account) const;
	QString getJid(int account) const;
	QString getId(int account) const;
	QString getName(int account) const;
	int findOnlineAccountForContact(const QString &jid) const;

	bool isSelf(int account, const QString& jid) const;
	bool isAgent(int account, const QString& jid) const;
	bool inList(int account, const QString& jid) const;
	bool isPrivate(int account, const QString& jid) const;
	bool isConference(int account, const QString& jid) const;
	QString name(int account, const QString& jid) const;
	QString status(int account, const QString& jid) const;
	QString statusMessage(int account, const QString& jid) const;
	QStringList resources(int account, const QString& jid) const;

	bool setActivity(int account, const QString& Jid, QDomElement xml);
	bool setMood(int account, const QString& Jid, QDomElement xml);
	bool setTune(int account, const QString& Jid, const QString& tune);

	void initPopup(const QString& text, const QString& title, const QString& icon, int type);
	void initPopupForJid(int account, const QString& jid, const QString& text, const QString& title, const QString& icon, int tipe);
	int registerOption(const QString& name, int initValue = 5, const QString& path = QString());
	void unregisterOption(const QString& name);
	int popupDuration(const QString& name) const;
	void setPopupDuration(const QString& name, int value);

	void setStatus(int account, const QString& status, const QString& statusMessage);

	bool appendSysMsg(int account, const QString& jid, const QString& message);

	void createNewEvent(int account, const QString& jid, const QString& descr, QObject *receiver, const char* slot);

	friend class PluginHost;

private slots:
	void dirsChanged();
	void optionChanged(const QString& option);
	void accountDestroyed();
};

#endif
