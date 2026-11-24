/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "filesharingresponder.h"
#include <QObject>
#include <QPointer>
#include <QFile>
#include <functional>
#include <iris/jingle-ft.h>
#include <iris/xmpp_file-sharing.h>
#include <iris/xmpp_reference.h>
#include <iris/xmpp_url.h>
namespace XMPP {
class Client;
class HttpFileUpload;
}

// Registers an owner-side Jingle factory and announces it in the owner's private
// PEP catalog. Local SIMS hash requests remain usable if PEP is unsupported.
bool publishSharedFile(XMPP::Client *client, const XMPP::Jingle::FileTransfer::File &file,
                       const FileSharingResponder::Lookup &lookup,
                       QObject *context, std::function<void(bool, const QString &)> announced);

// One attempt gathers all usable sources before the caller sends a SIMS message.
class FileSharingPublication : public QObject {
    Q_OBJECT
public:
    enum class Mode { Plain, Aes256Gcm };
    FileSharingPublication(XMPP::Client *client, const XMPP::Jingle::FileTransfer::File &file, const QString &path,
                           const XMPP::Jid &selfJid, QObject *parent = nullptr, Mode mode = Mode::Plain);
    void               start(const QStringList &cachedSources, const FileSharingResponder::Lookup &lookup = {});
    const QStringList &sources() const { return _sources; }
    XMPP::Reference encryptedReference() const { return _encryptedReference; }
    QString errorString() const { return _errorString; }

signals:
    void finished();
    void progress(qint64 bytes);
    void logMessage(const QString &message);

private:
    void checkHttpSources(const QStringList &sources);
    void upload();
    void uploadEncrypted();
    void checkFinished();

    QPointer<XMPP::Client>           _client;
    XMPP::Jingle::FileTransfer::File _file;
    QString                          _path;
    XMPP::Jid                        _selfJid;
    QStringList                      _sources;
    Mode                             _mode = Mode::Plain;
    QString                          _errorString;
    XMPP::Reference                  _encryptedReference;
    QFile                           *_plaintext = nullptr; // children of this publication
    XMPP::StatelessFileSharing::EncryptingDevice *_ciphertext = nullptr;
    QPointer<XMPP::HttpFileUpload>   _encryptedUploader;
    int                              _pendingHeads   = 0;
    bool                             _started        = false;
    bool                             _httpFinished   = false;
    bool                             _jingleFinished = false;
    bool                             _notified       = false;
};

// XEP-0066 compatibility for the HTTP sources of outgoing SIMS messages.
XMPP::UrlList fileSharingHttpUrls(const QList<XMPP::Reference> &references);
