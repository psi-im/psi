#ifndef PSIMEDIAACCESSOR_H
#define PSIMEDIAACCESSOR_H

#include <QtPlugin>

class PsiMediaHost;

class PsiMediaAccessor {
public:
    virtual ~PsiMediaAccessor() { }

    virtual void setPsiMediaHost(PsiMediaHost *host) = 0;
};

Q_DECLARE_INTERFACE(PsiMediaAccessor, "org.psi-im.PsiMediaAccessor/0.1");

#endif // PSIMEDIAACCESSOR_H
