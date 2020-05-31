#ifndef PSIACCOUNTCONTROLLINGHOST_H
#define PSIACCOUNTCONTROLLINGHOST_H

#include <QtPlugin>
#include <functional>

class QString;

class PsiAccountControllingHost {
public:
    virtual ~PsiAccountControllingHost() { }

    virtual void setStatus(int account, const QString &status, const QString &statusMessage) = 0;
    virtual void setPgpKey(int account, const QString &keyId) = 0;
    virtual void removeKnownPgpKey(int account, const QString &jid) = 0;

    virtual bool appendSysMsg(int account, const QString &jid, const QString &message)     = 0;
    virtual bool appendSysHtmlMsg(int account, const QString &jid, const QString &message) = 0;

    virtual void subscribeLogout(QObject *context, std::function<void(int account)> callback) = 0;
};

Q_DECLARE_INTERFACE(PsiAccountControllingHost, "org.psi-im.PsiAccountControllingHost/0.5")

#endif // PSIACCOUNTCONTROLLINGHOST_H
