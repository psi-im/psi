/* SPDX-License-Identifier: GPL-2.0-or-later */
// Real Prosody/XEP-0313 pagination exercised through the exact Iris
// MAMManager method and reverse-cursor order used by Psi BASIC chat.
#include <iris/xmpp.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>
#include <iris/xmpp_forwarding.h>
#include <iris/xmpp_mammanager.h>
#include <iris/xmpp_message.h>
#include <iris/xmpp_status.h>
#include <iris/xmpp_tasks.h>
#include <iris/xmpp_xmlcommon.h>

#include <QtCrypto>
#include <QCoreApplication>
#include <QDebug>
#include <QSet>
#include <QTimer>

#include <functional>

using namespace XMPP;

namespace {
constexpr int PageSize = 40;
constexpr int MessageCount = 85;

class Endpoint final : public QObject {
public:
    explicit Endpoint(QObject *parent = nullptr) : QObject(parent) { }

    ~Endpoint() override
    {
        if (stream_) {
            client_.close();
            delete stream_;
        }
        delete connector_;
    }

    Client *client() { return &client_; }

    void start(std::function<void()> ready, std::function<void(const QString &)> error)
    {
        ready_ = std::move(ready);
        error_ = std::move(error);
        connector_ = new AdvancedConnector;
        connector_->setOptHostPort(QStringLiteral("127.0.0.1"), 5222);
        connector_->setOptSSL(false);
        stream_ = new ClientStream(connector_, nullptr);
        stream_->setAllowPlain(ClientStream::AllowPlain);
        stream_->setRequireMutualAuth(false);
        stream_->setCompress(false);
        stream_->setNoopTime(0);

        connect(stream_, &ClientStream::needAuthParams, this,
                [this](bool user, bool pass, bool realm) {
            if (user) stream_->setUsername(QStringLiteral("user01"));
            if (pass) stream_->setPassword(QStringLiteral("ci-pass-01"));
            if (realm) stream_->setRealm(QStringLiteral("localhost"));
            stream_->continueAfterParams();
        });
        connect(stream_, &ClientStream::warning, this, [this](int) {
            stream_->continueAfterWarning(); // CI-only loopback plaintext server
        });
        connect(stream_, &ClientStream::authenticated, this, [this] {
            const Jid bound = stream_->jid();
            const QString resource = bound.resource().isEmpty()
                ? QStringLiteral("mam-probe") : bound.resource();
            client_.start(QStringLiteral("localhost"), QStringLiteral("user01"),
                          QStringLiteral("ci-pass-01"), resource);
            if (client_.isSessionRequired()) {
                auto *session = new JT_Session(client_.rootTask());
                connect(session, &Task::finished, this, [this, session] {
                    if (!session->success()) {
                        error_(QStringLiteral("session establishment failed"));
                        return;
                    }
                    becomeReady();
                });
                session->go(true);
            } else {
                becomeReady();
            }
        });
        connect(stream_, &Stream::error, this,
                [this](int code) {
            error_(QStringLiteral("stream error %1").arg(code));
        });
        client_.connectToServer(stream_, Jid(QStringLiteral("user01@localhost/mam-probe")));
    }

private:
    void becomeReady()
    {
        client_.setPresence(Status());
        if (ready_) ready_();
    }

    Client client_;
    AdvancedConnector *connector_ = nullptr;
    ClientStream *stream_ = nullptr;
    std::function<void()> ready_;
    std::function<void(const QString &)> error_;
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;
    Endpoint endpoint(&app);

    bool done = false;
    auto finish = [&](int code, const QString &message) {
        if (done) return;
        done = true;
        if (code)
            qCritical().noquote() << "MAM_RUNTIME_RESULT=failure" << message;
        else
            qInfo().noquote() << "MAM_RUNTIME_RESULT=success" << message;
        QTimer::singleShot(30, &app, [&, code] { app.exit(code); });
    };
    QTimer::singleShot(60000, &app, [&] {
        finish(124, QStringLiteral("60s MAM runtime timeout"));
    });

    int pageNo = 0;
    int visited = 0;
    QString before;
    QSet<QString> archiveUids;
    const Jid peer(QStringLiteral("user02@localhost"));

    std::function<void()> fetchPage;
    fetchPage = [&] {
        if (done) return;
        MAMManager manager(endpoint.client(), PageSize, PageSize, false, true);
        // getMessagesBeforeID hardcodes flipPages=true, so use the explicit
        // non-flipped chronological API just as PsiChatDlg does.
        auto *task = manager.getArchiveByIDRange(peer, QString(), before, false);
        QObject::connect(task, &Task::finished, &app, [&, task] {
            if (done) return;
            if (!task->success()) {
                finish(20, QStringLiteral("MAM page failed: %1").arg(task->statusString()));
                task->deleteLater();
                return;
            }
            const auto &archive = task->archive();
            const int expectedFirst = qMax(0, MessageCount - (pageNo + 1) * PageSize);
            const int expectedLast = MessageCount - pageNo * PageSize;
            if (archive.size() != expectedLast - expectedFirst) {
                finish(21, QStringLiteral("page %1 has %2 entries; expected %3")
                       .arg(pageNo).arg(archive.size()).arg(expectedLast - expectedFirst));
                task->deleteLater();
                return;
            }

            QString oldest;
            for (int index = 0; index < archive.size(); ++index) {
                const auto &result = archive.at(index);
                const QString uid = result.attribute(QStringLiteral("id"));
                if (uid.isEmpty() || archiveUids.contains(uid)) {
                    finish(22, QStringLiteral("archive UID missing or repeated"));
                    task->deleteLater();
                    return;
                }
                archiveUids.insert(uid);
                if (oldest.isEmpty()) oldest = uid;

                const auto forwarded = childElementsByTagNameNS(result,
                    QStringLiteral("urn:xmpp:forward:0"), QStringLiteral("forwarded"))
                                           .item(0).toElement();
                Forwarding wrapper;
                if (forwarded.isNull() || !wrapper.fromXml(forwarded, endpoint.client())) {
                    finish(23, QStringLiteral("forwarded archived message is invalid"));
                    task->deleteLater();
                    return;
                }
                const auto message = wrapper.message();
                const QString expectedText = QStringLiteral("psi-mam-ci-%1")
                                                 .arg(expectedFirst + index);
                if (message.body() != expectedText || message.from().bare() !=
                    QStringLiteral("user01@localhost") || message.to().bare() != peer.bare()) {
                    finish(24, QStringLiteral("wrong MAM order or peer on page %1 index %2: %3")
                           .arg(pageNo).arg(index).arg(message.body()));
                    task->deleteLater();
                    return;
                }
            }
            if (oldest.isEmpty() || oldest == before) {
                finish(25, QStringLiteral("opaque MAM cursor did not advance"));
                task->deleteLater();
                return;
            }
            before = oldest;
            visited += archive.size();
            const bool complete = task->isComplete();
            qInfo().noquote() << QStringLiteral("MAM_RUNTIME_PAGE=%1 count=%2 complete=%3")
                .arg(pageNo).arg(archive.size()).arg(complete);
            task->deleteLater();
            ++pageNo;
            if (pageNo == 3) {
                if (!complete || visited != MessageCount) {
                    finish(26, QStringLiteral("last MAM page not complete or count wrong"));
                    return;
                }
                finish(0, QStringLiteral("85 messages retrieved in three chronological pages"));
            } else if (complete) {
                finish(27, QStringLiteral("archive unexpectedly exhausted early"));
            } else {
                QTimer::singleShot(0, &app, fetchPage);
            }
        });
        task->go();
    };

    endpoint.start(
        [&] {
            qInfo() << "MAM_RUNTIME_AUTHENTICATED";
            for (int i = 0; i < MessageCount; ++i) {
                Message message(peer);
                message.setType(Message::Type::Chat);
                message.setId(QStringLiteral("psi-mam-msg-%1").arg(i));
                message.setBody(QStringLiteral("psi-mam-ci-%1").arg(i));
                endpoint.client()->sendMessage(message);
            }
            // Allow Prosody to persist the queued stanzas before querying.
            QTimer::singleShot(1500, &app, fetchPage);
        },
        [&](const QString &err) { finish(10, err); });
    return app.exec();
}
