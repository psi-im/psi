// SPDX-License-Identifier: LGPL-2.1-or-later
#include "screencapture.h"
#include "psimediaprovider.h"
#include "psimediascreencapture.h"
#include <QPointer>

namespace PsiMedia {
Provider *provider();
class ScreenCapture::Private {
public:
    ScreenCaptureContext *context = nullptr;
    QPointer<QObject>     object;
};
ScreenCapture::ScreenCapture(QObject *parent) : QObject(parent), d(new Private) { }
ScreenCapture::~ScreenCapture()
{
    if (d->object) {
        d->object->disconnect(this);
        delete d->context;
    }
    delete d;
}
bool ScreenCapture::available()
{
    auto *p         = provider();
    auto *extension = p && p->isInitialized() ? qobject_cast<ScreenCaptureProvider *>(p->qobject()) : nullptr;
    return extension && extension->screenCaptureAvailable();
}
void ScreenCapture::startSelection()
{
    if (!d->object) {
        auto *p         = provider();
        auto *extension = p && p->isInitialized() ? qobject_cast<ScreenCaptureProvider *>(p->qobject()) : nullptr;
        if (!extension || !(d->context = extension->createScreenCapture())) {
            emit failed(tr("The media backend does not support screen sharing."));
            return;
        }
        d->object = d->context->qobject();
        d->object->setParent(this);
        connect(d->object, SIGNAL(sourcesAvailable()), this, SIGNAL(sourcesAvailable()));
        connect(d->object, SIGNAL(ready(QString, QString)), this, SIGNAL(ready(QString, QString)));
        connect(d->object, SIGNAL(stopped()), this, SIGNAL(stopped()));
        connect(d->object, SIGNAL(failed(QString)), this, SIGNAL(failed(QString)));
        connect(p->qobject(), &QObject::destroyed, this, [this] {
            if (d->object) {
                QPointer<ScreenCapture> guard(this);
                d->context->stop();
                if (!guard)
                    return;
                if (d->object)
                    delete d->context;
                d->context = nullptr;
            }
        });
    }
    d->context->startSelection();
}
QList<ScreenCapture::Source> ScreenCapture::sources() const
{
    QList<Source> result;
    if (d->object)
        for (const auto &source : d->context->sources())
            result.append({ source.kind == PScreenCaptureSource::Display ? Source::Display : Source::Window, source.id,
                            source.label, source.thumbnail });
    return result;
}
void ScreenCapture::choose(const QString &id)
{
    if (d->object)
        d->context->choose(id);
}
void ScreenCapture::stop()
{
    if (d->object)
        d->context->stop();
}
std::shared_ptr<const void> ScreenCapture::sourceLease() const
{
    return d->object ? d->context->sourceLease() : std::shared_ptr<const void>();
}
}
