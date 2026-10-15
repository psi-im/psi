#pragma once

#include "mediachunktypes.h"
#include <QObject>
#include <functional>
#include <memory>

namespace XMPP {
class Jid;
namespace Jingle {
    class Manager;
    namespace FileTransfer {
        class Application;
        class File;
    }
}
}

// Account-scoped queue of bounded media requests. Only negotiated BUNDLE
// transports are reused. Ordinary downloads do not enter this queue.
class FileSharingJingleSession : public QObject {
public:
    using Ready = std::function<void(XMPP::Jingle::FileTransfer::Application *)>;
    explicit FileSharingJingleSession(XMPP::Jingle::Manager *manager, QObject *parent = nullptr);
    ~FileSharingJingleSession() override;

    // Ready runs before prepare/initiate, allowing the consumer to install its
    // streaming callbacks. Cancellation never cancels another queued request.
    MediaChunkAbort request(const XMPP::Jid &peer, const XMPP::Jingle::FileTransfer::File &file, QObject *consumer,
                            Ready ready);

private:
    class Private;
    std::unique_ptr<Private> d;
};
