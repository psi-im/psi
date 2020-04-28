#ifndef OPTIONACCESSINGHOST_H
#define OPTIONACCESSINGHOST_H

#include <QVariant>
#include <QtPlugin>
#include <functional>

class QString;
class QVariant;

class OAH_PluginOptionsTab {
public:
    virtual QWidget *widget() = 0; // caller will take ownership of the plugin but it still can be deleted externally
    virtual void     applyOptions()   = 0;
    virtual void     restoreOptions() = 0;

    virtual QByteArray id() const       = 0; // Unique identifier, i.e. "plugins_misha's_cool-plugin"
    virtual QByteArray nextToId() const = 0; // the page will be added after this page
    virtual QByteArray parentId() const = 0; // Identifier of parent tab, i.e. "general"

    virtual QString title() const = 0; // "General"
    virtual QIcon   icon() const  = 0;
    virtual QString desc() const  = 0; // "You can configure your roster here"

    virtual void setCallbacks(std::function<void()> dataChanged, std::function<void(bool)> noDirty,
                              std::function<void(QWidget *)> connectDataChanged)
        = 0; // basically signals. calling callbacks with uninitalized widget is undefined behavior
};

class OptionAccessingHost {
public:
    virtual ~OptionAccessingHost() { }

    virtual void     setPluginOption(const QString &option, const QVariant &value)                        = 0;
    virtual QVariant getPluginOption(const QString &option, const QVariant &defValue = QVariant::Invalid) = 0;

    virtual void     setGlobalOption(const QString &option, const QVariant &value) = 0;
    virtual QVariant getGlobalOption(const QString &option)                        = 0;
    virtual void     addSettingPage(OAH_PluginOptionsTab *tab)                     = 0;
    virtual void     removeSettingPage(OAH_PluginOptionsTab *tab)                  = 0;
};

Q_DECLARE_INTERFACE(OptionAccessingHost, "org.psi-im.OptionAccessingHost/0.1");

#endif // OPTIONACCESSINGHOST_H
