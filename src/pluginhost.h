/*
 * Copyright (C) 2006  Kevin Smith
 * Copyright (C) 2008  Maciej Niedzielski
 */

#ifndef PLUGINHOST_H
#define PLUGINHOST_H

#include "accountinfoaccessinghost.h"
#include "activetabaccessinghost.h"
#include "applicationinfo.h"
#include "applicationinfoaccessinghost.h"
#include "contactinfoaccessinghost.h"
#include "contactstateaccessinghost.h"
#include "encryptionsupport.h"
#include "eventcreatinghost.h"
#include "iconfactoryaccessinghost.h"
#include "iconset.h"
#include "iqfilteringhost.h"
#include "optionaccessinghost.h"
#include "pluginaccessinghost.h"
#include "popupaccessinghost.h"
#include "psiaccountcontrollinghost.h"
#include "psimediahost.h"
#include "shortcutaccessinghost.h"
#include "soundaccessinghost.h"
#include "stanzasendinghost.h"
#include "tabbablewidget.h"
#include "tabdlg.h"
#include "userlist.h"
#include "webkitaccessinghost.h"

#include <QDomElement>
#include <QMultiMap>
#include <QPointer>
#include <QRegExp>
#include <QTextEdit>
#include <QVariant>

class IqNamespaceFilter;
class PluginManager;
class QPluginLoader;
class QWidget;
namespace PsiMedia {
class Provider;
}

class PluginHost : public QObject,
                   public StanzaSendingHost,
                   public IqFilteringHost,
                   public OptionAccessingHost,
                   public ShortcutAccessingHost,
                   public IconFactoryAccessingHost,
                   public ActiveTabAccessingHost,
                   public ApplicationInfoAccessingHost,
                   public AccountInfoAccessingHost,
                   public PopupAccessingHost,
                   public ContactStateAccessingHost,
                   public PsiAccountControllingHost,
                   public EventCreatingHost,
                   public ContactInfoAccessingHost,
                   public SoundAccessingHost,
                   public EncryptionSupport,
                   public PluginAccessingHost,
                   public WebkitAccessingHost,
                   public PsiMediaHost {
    Q_OBJECT
    Q_INTERFACES(StanzaSendingHost IqFilteringHost OptionAccessingHost ShortcutAccessingHost IconFactoryAccessingHost
                     ActiveTabAccessingHost ApplicationInfoAccessingHost AccountInfoAccessingHost PopupAccessingHost
                         ContactStateAccessingHost PsiAccountControllingHost EventCreatingHost ContactInfoAccessingHost
                             SoundAccessingHost EncryptionSupport PluginAccessingHost WebkitAccessingHost PsiMediaHost)

public:
    PluginHost(PluginManager *manager, const QString &pluginFile);
    virtual ~PluginHost();

    bool           isValid() const;
    const QString &path() const;
    QWidget *      optionsWidget() const;

    // cached basic info
    const QString &name() const;
    const QString &shortName() const;
    const QString &version() const;
    int            priority() const;
    const QIcon &  icon() const;

    QStringList pluginFeatures() const;

    // loading
    void updateMetadata();
    bool load();
    bool unload();
    bool isLoaded() const;

    // enabling
    bool enable();
    bool disable();
    bool isEnabled() const;

    // for StanzaFilter and IqNamespaceFilter
    bool incomingXml(int account, const QDomElement &e);
    bool outgoingXml(int account, QDomElement &e);

    // for EventFilter
    bool processEvent(int account, QDomElement &e);
    bool processMessage(int account, const QString &jidFrom, const QString &body, const QString &subject);
    bool processOutgoingMessage(int account, const QString &jidTo, QString &body, const QString &type,
                                QString &subject);
    void logout(int account);

    // StanzaSendingHost
    void    sendStanza(int account, const QDomElement &xml) override;
    void    sendStanza(int account, const QString &xml) override;
    void    sendMessage(int account, const QString &to, const QString &body, const QString &subject,
                        const QString &type) override;
    QString uniqueId(int account) override;
    QString escape(const QString &str) override;

    // IqFilteringHost
    void addIqNamespaceFilter(const QString &ns, IqNamespaceFilter *filter) override;
    void addIqNamespaceFilter(const QRegExp &ns, IqNamespaceFilter *filter) override;
    void removeIqNamespaceFilter(const QString &ns, IqNamespaceFilter *filter) override;
    void removeIqNamespaceFilter(const QRegExp &ns, IqNamespaceFilter *filter) override;

    // OptionAccessingHost
    void     setPluginOption(const QString &option, const QVariant &value) override;
    QVariant getPluginOption(const QString &option, const QVariant &defValue = QVariant::Invalid) override;
    void     setGlobalOption(const QString &option, const QVariant &value) override;
    QVariant getGlobalOption(const QString &option) override;
    void     optionChanged(const QString &option);
    void     addSettingPage(OAH_PluginOptionsTab *tab) override;
    void     removeSettingPage(OAH_PluginOptionsTab *tab) override;

    // ShortcutAccessingHost
    void setShortcuts();
    void connectShortcut(const QKeySequence &shortcut, QObject *receiver, const char *slot) override;
    void disconnectShortcut(const QKeySequence &shortcut, QObject *receiver, const char *slot) override;
    void requestNewShortcut(QObject *receiver, const char *slot) override;

    // IconFacrotyAccessingHost
    QIcon getIcon(const QString &name) override;
    void  addIcon(const QString &name, const QByteArray &icon) override;

    // ActiveTabHost
    QTextEdit *getEditBox() override;
    QString    getJid() override;
    QString    getYourJid() override;

    // ApplicationInfoAccessingHost
    Proxy   getProxyFor(const QString &obj) override;
    QString appName() override;
    QString appVersion() override;
    QString appCapsNode() override;
    QString appCapsVersion() override;
    QString appOsName() override;
    QString appHomeDir(HomedirType type) override;
    QString appResourcesDir() override;
    QString appLibDir() override;
    QString appProfilesDir(HomedirType type) override;
    QString appHistoryDir() override;
    QString appCurrentProfileDir(HomedirType type) override;
    QString appVCardDir() override;

    // AccountInfoAcsessingHost
    QString     getStatus(int account) override;
    QString     getStatusMessage(int account) override;
    QString     proxyHost(int account) override;
    int         proxyPort(int account) override;
    QString     proxyUser(int account) override;
    QString     proxyPassword(int account) override;
    QString     getJid(int account) override;
    QString     getId(int account) override;
    QString     getName(int account) override;
    QStringList getRoster(int account) override;
    int         findOnlineAccountForContact(const QString &jid) const override;
    QString     getPgpKey(int account) override;

    // ContactInfoAccessingHost
    bool        isSelf(int account, const QString &jid) override;
    bool        isAgent(int account, const QString &jid) override;
    bool        inList(int account, const QString &jid) override;
    bool        isPrivate(int account, const QString &jid) override;
    bool        isConference(int account, const QString &jid) override;
    QString     name(int account, const QString &jid) override;
    QString     status(int account, const QString &jid) override;
    QString     statusMessage(int account, const QString &jid) override;
    QStringList resources(int account, const QString &jid) override;
    QString     realJid(int account, const QString &jid) override;
    QStringList mucNicks(int account, const QString &mucJid) override;
    bool        hasCaps(int account, const QString &jid, const QStringList &caps) override;

    // ContactStateAccessor
    bool setActivity(int account, const QString &Jid, QDomElement xml) override;
    bool setMood(int account, const QString &Jid, QDomElement xml) override;
    bool setTune(int account, const QString &Jid, QString tune) override;

    // PopupAccessingHost
    void initPopup(const QString &text, const QString &title, const QString &icon, int type) override;
    void initPopupForJid(int account, const QString &jid, const QString &text, const QString &title,
                         const QString &icon, int type) override;
    int  registerOption(const QString &name, int initValue = 5, const QString &path = QString()) override;
    int  popupDuration(const QString &name) override;
    void setPopupDuration(const QString &name, int value) override;
    void unregisterOption(const QString &name) override;

    void addToolBarButton(QObject *parent, QWidget *toolbar, int account, const QString &contact);
    bool hasToolBarButton();

    void addGCToolBarButton(QObject *parent, QWidget *toolbar, int account, const QString &contact);
    bool hasGCToolBarButton();

    void addAccountMenu(QMenu *menu, int account);
    void addContactMenu(QMenu *menu, int account, const QString &jid);

    // ChatTabAccessor
    void setupChatTab(QWidget *tab, int account, const QString &contact);

    // GCTabAccessor
    void setupGCTab(QWidget *tab, int account, const QString &contact);

    bool appendingChatMessage(int account, const QString &contact, QString &body, QDomElement &html, bool local);

    void applyOptions();
    void restoreOptions();

    QString pluginInfo();
    bool    hasInfoProvider();

    // PsiAccountControllingHost
    void setStatus(int account, const QString &status, const QString &statusMessage) override;
    bool appendSysMsg(int account, const QString &jid, const QString &message) override;
    bool appendSysHtmlMsg(int account, const QString &jid, const QString &message) override;
    bool appendMsg(int account, const QString &jid, const QString &message, const QString &id,
                   bool wasEncrypted) override;
    void subscribeLogout(QObject *context, std::function<void(int account)> callback) override;
    void setPgpKey(int account, const QString &keyId) override;

    void createNewEvent(int account, const QString &jid, const QString &descr, QObject *receiver,
                        const char *slot) override;
    void createNewMessageEvent(int account, QDomElement const &element) override;

    void playSound(const QString &fileName) override;

    // EncryptionSupport
    bool decryptMessageElement(int account, QDomElement &message) override;
    bool encryptMessageElement(int account, QDomElement &message) override;

    // PluginAccessingHost

    QObject *getPlugin(const QString &name) override;

    // WebkitAccessingHost
    RenderType chatLogRenderType() const override;
    QString    installChatLogJSDataFilter(const QString &     js,
                                          PsiPlugin::Priority priority = PsiPlugin::PriorityNormal) override;
    void       uninstallChatLogJSDataFilter(const QString &id) override;
    void       executeChatLogJavaScript(QWidget *log, const QString &js) override;

    // PsiMediaHost (calls from plugin to psi)
    void selectMediaDevices(const QString &audioInput, const QString &audioOutput, const QString &videoInput) override;
    void setMediaProvider(PsiMedia::Provider *provider) override;

private:
    PluginManager *   manager_ = nullptr;
    QPointer<QObject> plugin_;
    QString           file_;
    QString           name_;
    QString           shortName_;
    QString           version_;
    int               priority_ = 0;
    QIcon             icon_;
    QPluginLoader *   loader_             = nullptr;
    Iconset *         iconset_            = nullptr;
    bool              hasToolBarButton_   = false;
    bool              hasGCToolBarButton_ = false;

    bool    valid_     = false;
    bool    connected_ = false;
    bool    enabled_   = false;
    bool    hasInfo_   = false;
    QString infoString_;

    QMultiMap<QString, IqNamespaceFilter *> iqNsFilters_;
    QMultiMap<QRegExp, IqNamespaceFilter *> iqNsxFilters_;
    QList<QVariantHash>                     buttons_;
    QList<QVariantHash>                     gcbuttons_;

    QList<QVariantHash> accMenu_;
    QList<QVariantHash> contactMenu_;

    bool loadPlugin(QObject *pluginObject);

    // disable copying
    PluginHost(const PluginHost &);
    PluginHost &operator=(const PluginHost &);
};

#endif // PLUGINHOST_H
