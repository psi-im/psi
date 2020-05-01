#ifndef PSIMEDIAHOST_H
#define PSIMEDIAHOST_H

#include <QtPlugin>
#include <functional>

class QString;

class PsiMediaHost {
public:
    virtual ~PsiMediaHost() { }

    // select mic, speeakers and webcam
    virtual void selectMediaDevices(const QString &audioInput, const QString &audioOutput, const QString &videoInput)
        = 0;
};

Q_DECLARE_INTERFACE(PsiMediaHost, "org.psi-im.PsiMediaHost/0.1")

#endif // PSIMEDIAHOST_H
