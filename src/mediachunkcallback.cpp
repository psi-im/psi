#include "mediachunkcallback.h"
#include <QThread>
#include <memory>
#include <utility>

namespace {
class CompletionRelay : public QObject {
    Q_OBJECT
public:
    void deliver(QByteArray bytes, MediaChunkError error) { emit completed(std::move(bytes), std::move(error)); }
signals:
    void completed(QByteArray bytes, MediaChunkError error);
};
}

std::function<void(QByteArray, MediaChunkError)>
queuedMediaChunkCompletion(QObject *receiver, std::function<void(QByteArray, MediaChunkError)> done)
{
    qRegisterMetaType<MediaChunkError>();
    auto relay = std::shared_ptr<CompletionRelay>(new CompletionRelay, [](CompletionRelay *p) {
        if (p->thread() == QThread::currentThread())
            delete p;
        else
            p->deleteLater();
    });
    QObject::connect(relay.get(), &CompletionRelay::completed, receiver, std::move(done), Qt::QueuedConnection);
    return [relay](QByteArray bytes, MediaChunkError error) { relay->deliver(std::move(bytes), std::move(error)); };
}

#include "mediachunkcallback.moc"
