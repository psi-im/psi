// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <memory>

namespace PsiMedia {
// Optional screen capture extension. IDs are provider-owned opaque device IDs,
// passed unchanged to RtpSessionContext::setVideoInputDevice(). Neither native
// handles nor media pipeline descriptions cross this interface.
struct PScreenCaptureSource {
    enum Kind { Display, Window } kind = Display;
    QString    id;
    QString    label;
    QByteArray thumbnail; // optional PNG preview, independent of the media backend
};

class ScreenCaptureContext {
public:
    virtual ~ScreenCaptureContext() { }
    virtual QObject *qobject() = 0;
    // All calls and signals use the QObject owner (GUI) thread. Explicit user
    // action is required before startSelection(), which may request OS consent
    // and present a system picker. It never starts media transmission.
    virtual void                        startSelection()          = 0;
    virtual QList<PScreenCaptureSource> sources() const           = 0;
    virtual void                        choose(const QString &id) = 0;
    // Revokes permission and invalidates issued device IDs. Consumers must stop
    // capture synchronously on stopped(). A lease keeps resources valid until
    // the previous media graph has actually released its source.
    virtual void                        stop()              = 0;
    virtual std::shared_ptr<const void> sourceLease() const = 0;

    // The QObject supplies signals sourcesAvailable(), ready(QString,QString),
    // stopped(), and failed(QString). No platform details cross those signals.
};

class ScreenCaptureProvider {
public:
    virtual ~ScreenCaptureProvider() { }
    // A permission-free capability probe. It must not enumerate windows or
    // trigger a desktop permission dialog.
    virtual bool screenCaptureAvailable() const = 0;
    // Parentless context, owned by the caller; destroy before plugin unload.
    virtual ScreenCaptureContext *createScreenCapture() = 0;
};

}
Q_DECLARE_INTERFACE(PsiMedia::ScreenCaptureProvider, "org.psi-im.psimedia.ScreenCaptureProvider/1.0")
