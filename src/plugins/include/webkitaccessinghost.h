#ifndef WEBKITACCESSINGHOST_H
#define WEBKITACCESSINGHOST_H

class QString;

class WebkitAccessingHost
{
public:
    virtual ~WebkitAccessingHost() {}

    virtual QString installMessageViewJSFilter(const QString& js) = 0;
    virtual void uninstallMessageViewJSFilter(const QString &id) = 0;
};

Q_DECLARE_INTERFACE(WebkitAccessingHost, "org.psi-im.WebkitAccessingHost/0.1");

#endif // WEBKITACCESSINGHOST_H
