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
#include <QPointer>
#include <QTextEdit>

#include "applicationinfo.h"
#include "userlist.h"
#include "iconset.h"
#include "stanzasendinghost.h"
#include "iqfilteringhost.h"
#include "optionaccessinghost.h"
#include "shortcutaccessinghost.h"
#include "iconfactoryaccessinghost.h"
#include "activetabaccessinghost.h"
#include "applicationinfoaccessinghost.h"
#include "accountinfoaccessinghost.h"
#include "tabdlg.h"
#include "tabbablewidget.h"
#include "popupaccessinghost.h"
#include "contactstateaccessinghost.h"
#include "psiaccountcontrollinghost.h"
#include "eventcreatinghost.h"
#include "contactinfoaccessinghost.h"
#include "soundaccessinghost.h"

class QWidget;
class QPluginLoader;

class PluginManager;
class IqNamespaceFilter;

class PluginHost: public QObject, public StanzaSendingHost, public IqFilteringHost, public OptionAccessingHost, public ShortcutAccessingHost, public IconFactoryAccessingHost,
	public ActiveTabAccessingHost, public ApplicationInfoAccessingHost, public AccountInfoAccessingHost, public PopupAccessingHost, public ContactStateAccessingHost
	, public PsiAccountControllingHost, public EventCreatingHost, public ContactInfoAccessingHost, public SoundAccessingHost
{
	Q_OBJECT
	Q_INTERFACES(StanzaSendingHost IqFilteringHost OptionAccessingHost ShortcutAccessingHost IconFactoryAccessingHost
		ActiveTabAccessingHost ApplicationInfoAccessingHost AccountInfoAccessingHost PopupAccessingHost ContactStateAccessingHost
		PsiAccountControllingHost EventCreatingHost ContactInfoAccessingHost SoundAccessingHost)

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
	int priority() const;
	const QIcon& icon() const;

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
	bool outgoingXml(int account, QDomElement &e);

	// for EventFilter
	bool processEvent(int account, QDomElement& e);
	bool processMessage(int account, const QString& jidFrom, const QString& body, const QString& subject);
	bool processOutgoingMessage(int account, const QString& jidTo, QString& body, const QString& type, QString& subject);
	void logout(int account);

	// StanzaSendingHost
	void sendStanza(int account, const QDomElement& xml);
	void sendStanza(int account, const QString& xml);
	void sendMessage(int account, const QString& to, const QString& body, const QString& subject, const QString& type);
	QString uniqueId(int account);
	QString escape(const QString &str);

	// IqFilteringHost
	void addIqNamespaceFilter(const QString& ns, IqNamespaceFilter* filter);
	void addIqNamespaceFilter(const QRegExp& ns, IqNamespaceFilter* filter);
	void removeIqNamespaceFilter(const QString& ns, IqNamespaceFilter* filter);
	void removeIqNamespaceFilter(const QRegExp& ns, IqNamespaceFilter* filter);

	// OptionAccessingHost
	void setPluginOption(const QString& option, const QVariant& value);
	QVariant getPluginOption(const QString &option, const QVariant &defValue = QVariant::Invalid);
	void setGlobalOption(const QString& option, const QVariant& value);
	QVariant getGlobalOption(const QString& option);
	void optionChanged(const QString& option);

	// ShortcutAccessingHost
	void setShortcuts();
	void connectShortcut(const QKeySequence& shortcut, QObject *receiver, const char* slot) ;
	void disconnectShortcut(const QKeySequence& shortcut, QObject *receiver, const char* slot);
	void requestNewShortcut(QObject *receiver, const char* slot);

	// IconFacrotyAccessingHost
	QIcon getIcon(const QString& name);
	void addIcon(const QString& name, const QByteArray& icon);

	//ActiveTabHost
	QTextEdit* getEditBox();
	QString getJid();
	QString getYourJid();

	//ApplicationInfoAccessingHost
	Proxy getProxyFor(const QString &obj);
	QString appName();
	QString appVersion();
	QString appCapsNode();
	QString appCapsVersion();
	QString appOsName();
	QString appHomeDir(HomedirType type);
	QString appResourcesDir();
	QString appLibDir();
	QString appProfilesDir(HomedirType type);
	QString appHistoryDir();
	QString appCurrentProfileDir(HomedirType type);
	QString appVCardDir();

	//AccountInfoAcsessingHost
	QString getStatus(int account);
	QString getStatusMessage(int account);
	QString proxyHost(int account);
	int proxyPort(int account);
	QString proxyUser(int account);
	QString proxyPassword(int account);
	QString getJid(int account);
	QString getId(int account);
	QString getName(int account);
	QStringList getRoster(int account);
	int findOnlineAccountForContact(const QString &jid) const;

	//ContactInfoAccessingHost
	bool isSelf(int account, const QString& jid);
	bool isAgent(int account, const QString& jid);
	bool inList(int account, const QString& jid);
	bool isPrivate(int account, const QString& jid);
	bool isConference(int account, const QString& jid);
	QString name(int account, const QString& jid);
	QString status(int account, const QString& jid);
	QString statusMessage(int account, const QString& jid);
	QStringList resources(int account, const QString& jid);

	//ContactStateAccessor
	bool setActivity(int account, const QString& Jid, QDomElement xml);
	bool setMood(int account, const QString& Jid, QDomElement xml);
	bool setTune(int account, const QString& Jid, QString tune);

	//PopupAccessingHost
	void initPopup(const QString& text, const QString& title, const QString& icon, int type);
	void initPopupForJid(int account, const QString& jid, const QString& text, const QString& title, const QString& icon, int type);
	int registerOption(const QString& name, int initValue = 5, const QString& path = QString());
	int popupDuration(const QString& name);
	void setPopupDuration(const QString& name, int value);
	void unregisterOption(const QString& name);

	void addToolBarButton(QObject* parent, QWidget* toolbar, int account, const QString& contact);
	bool hasToolBarButton();

	void addGCToolBarButton(QObject* parent, QWidget* toolbar, int account, const QString& contact);
	bool hasGCToolBarButton();

	void addAccountMenu(QMenu *menu, int account);
	void addContactMenu(QMenu *menu, int account, const QString& jid);

	//ChatTabAccessor
	void setupChatTab(QWidget* tab, int account, const QString& contact);

	// GCTabAccessor
	void setupGCTab(QWidget* tab, int account, const QString& contact);

	bool appendingChatMessage(int account, const QString& contact,
				  QString& body, QDomElement& html, bool local);

	void applyOptions();
	void restoreOptions();

	QString pluginInfo();
	bool hasInfoProvider();

	void setStatus(int account, const QString& status, const QString& statusMessage);

	bool appendSysMsg(int account, const QString& jid, const QString& message);

	void createNewEvent(int account, const QString& jid, const QString& descr, QObject *receiver, const char* slot);

	void playSound(const QString& fileName);

private:
	PluginManager* manager_;
	QPointer<QObject> plugin_;
	QString file_;
	QString name_;
	QString shortName_;
	QString version_;
	int priority_;
	QIcon icon_;
	QPluginLoader* loader_;
	Iconset* iconset_;
	bool hasToolBarButton_;
	bool hasGCToolBarButton_;

	bool valid_;
	bool connected_;
	bool enabled_;
	bool hasInfo_;
	QString infoString_;

	QMultiMap<QString, IqNamespaceFilter*> iqNsFilters_;
	QMultiMap<QRegExp, IqNamespaceFilter*> iqNsxFilters_;
	QList< QVariantHash > buttons_;
	QList< QVariantHash > gcbuttons_;

	QList< QVariantHash > accMenu_;
	QList< QVariantHash > contactMenu_;

	bool loadPlugin(QObject* pluginObject);

	// disable copying
	PluginHost(const PluginHost&);
	PluginHost& operator=(const PluginHost&);
};


#endif
