/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "historyarchive.h"
#include <QDomElement>
#include <QObject>
#include <QPointer>

class PsiAccount;
namespace XMPP {
class MAMTask;
}

// One bounded history page. Keeps archive policy outside Iris and presentation
// outside EDB; both private chats and room timelines use the same cache path.
class HistorySync final : public QObject {
    Q_OBJECT
public:
    static constexpr int PageSize = 40;
    HistorySync(PsiAccount *, const QString &peer, bool room, const QString &before, QObject *parent);
    ~HistorySync() override;
    void               go();
    void               safeDelete();
    bool               success() const { return success_; }
    bool               isComplete() const { return page_.complete; }
    bool               isStable() const { return page_.stable; }
    bool               fromCache() const { return cached_; }
    QString            statusString() const { return error_; }
    QList<QDomElement> archive() const;
    qint64             eventId(const QString &uid) const;

signals:
    void finished();

private:
    bool                    parseResults(const QList<QDomElement> &, HistoryArchive::Page &);
    PsiAccount             *account_;
    HistoryArchive::Key     key_;
    QString                 before_, error_;
    QPointer<XMPP::MAMTask> task_;
    HistoryArchive::Page    page_;
    bool                    success_ = false, cached_ = false, cancelled_ = false;
};
