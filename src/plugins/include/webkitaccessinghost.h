#ifndef WEBKITACCESSINGHOST_H
#define WEBKITACCESSINGHOST_H

#include "psiplugin.h"

class QString;

class WebkitAccessingHost {
public:
    enum RenderType {
        RT_TextEdit,
        RT_WebKit,
        RT_WebEngine,
        RT_QML /* well it's reserved =) */
    };

    virtual ~WebkitAccessingHost() {}

    virtual RenderType chatLogRenderType() const = 0;

    // embed QObject into specified log widget (note, actual log widget could be a child. it will be found by "log"
    // name) virtual void embedChatLogJavaScriptObject(QWidget *log, QObject *object) = 0; // to early for this. needs
    // chatview reinitialization feature (QWebChannel doesn't support embed in runtime) which requires proper history
    // reloading.

    // installs global chat log data filter, where data is all the possible message types added to chatlog
    virtual QString installChatLogJSDataFilter(const QString &     js,
                                               PsiPlugin::Priority priority = PsiPlugin::PriorityNormal)
        = 0;
    // uninstall global chat log data filter
    virtual void uninstallChatLogJSDataFilter(const QString &id) = 0;

    // execute javascript in subscope of global. visible variable: "chat"
    virtual void executeChatLogJavaScript(QWidget *log, const QString &js) = 0;
};

Q_DECLARE_INTERFACE(WebkitAccessingHost, "org.psi-im.WebkitAccessingHost/0.1");

#endif // WEBKITACCESSINGHOST_H
