#ifndef PSIMEDIAHOST_H
#define PSIMEDIAHOST_H

#include "psimediaprovider.h"

#include <QtPlugin>
#include <functional>

class QString;

class PsiMediaHost {
public:
    virtual ~PsiMediaHost() { }

    // select mic, speeakers and webcam
    virtual void setMediaProvider(PsiMedia::Provider *provider) = 0;
    //    virtual void setMediaDevices(const QList<PsiMedia::PDevice> &audioOutput,
    //                                 const QList<PsiMedia::PDevice> &audioInput,
    //                                 const QList<PsiMedia::PDevice> &videoInput);
    virtual void selectMediaDevices(const QString &audioInput, const QString &audioOutput, const QString &videoInput)
        = 0;
};

Q_DECLARE_INTERFACE(PsiMediaHost, "org.psi-im.PsiMediaHost/0.1")

#endif // PSIMEDIAHOST_H
