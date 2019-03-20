#ifndef PSIACCOUNTCONTROLLINGHOST_H
#define PSIACCOUNTCONTROLLINGHOST_H

class QString;

class PsiAccountControllingHost
{
public:
    virtual ~PsiAccountControllingHost() {}

    virtual void setStatus(int account, const QString& status, const QString& statusMessage) = 0;

    virtual bool appendSysMsg(int account, const QString& jid, const QString& message) = 0;
    virtual bool appendSysHtmlMsg(int account, const QString& jid, const QString& message) = 0;

    virtual bool appendMsg(int account, const QString& jid, const QString& message, const QString& id, bool wasEncrypted = false) = 0;
};

Q_DECLARE_INTERFACE(PsiAccountControllingHost, "org.psi-im.PsiAccountControllingHost/0.2")

#endif // PSIACCOUNTCONTROLLINGHOST_H
