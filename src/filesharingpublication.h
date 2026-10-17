/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QObject>
#include <QPointer>
#include <functional>
#include <iris/jingle-ft.h>
#include <iris/xmpp_reference.h>
#include <iris/xmpp_url.h>
namespace XMPP {
class Client;
}

// Registers an owner-side Jingle factory and announces it in the owner's private
// PEP catalog. Local SIMS hash requests remain usable if PEP is unsupported.
bool publishSharedFile(XMPP::Client *client, const XMPP::Jingle::FileTransfer::File &file, const QString &path,
                       QObject *context, std::function<void(bool, const QString &)> announced);

// One attempt gathers all usable sources before the caller sends a SIMS message.
class FileSharingPublication : public QObject {
    Q_OBJECT
public:
    FileSharingPublication(XMPP::Client *client, const XMPP::Jingle::FileTransfer::File &file, const QString &path,
                           const XMPP::Jid &selfJid, QObject *parent = nullptr);
    void               start(const QStringList &cachedSources, bool serveLocally);
    const QStringList &sources() const { return _sources; }

signals:
    void finished();
    void progress(qint64 bytes);
    void logMessage(const QString &message);

private:
    void checkHttpSources(const QStringList &sources);
    void upload();
    void checkFinished();

    QPointer<XMPP::Client>           _client;
    XMPP::Jingle::FileTransfer::File _file;
    QString                          _path;
    XMPP::Jid                        _selfJid;
    QStringList                      _sources;
    int                              _pendingHeads   = 0;
    bool                             _started        = false;
    bool                             _httpFinished   = false;
    bool                             _jingleFinished = false;
    bool                             _notified       = false;
};

// XEP-0066 compatibility for the HTTP sources of outgoing SIMS messages.
XMPP::UrlList fileSharingHttpUrls(const QList<XMPP::Reference> &references);
