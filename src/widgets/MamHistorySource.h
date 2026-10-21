/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "messageview.h"
#include <QHash>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QSet>
#include <QStringList>
#include <QVector>

class MamChatWidget;

// Logical archive topology and resident message data. No widgets, pixels,
// scrollbar policy or XMPP XML: the view supplies demand and the transport
// supplies a bounded, decoded page. RSM positions describe events, never rows.
class MamHistorySource {
public:
    static constexpr int MinRequestEvents = 15;
    static constexpr int MaxRequestEvents = 40;
    struct Row {
        QString       key;
        MessageView   message;
        QSet<QString> stanzaIds;
    };
    struct Window {
        int  first = -1, last = -1;
        bool serverReported = false, stable = true;
    };
    struct Request {
        enum Kind { None, Before, After, Index } kind = None;
        QString cursor;
        int     index = -1;
        int     limit = MaxRequestEvents, snapshotCount = -1;
        int     first = -1, last = -1;
        quint64 generation = 0;
    };

    const QList<Row> &rows() const { return rows_; }
    void              insertRow(int at, const Row &row)
    {
        rows_.insert(at, row);
        rebuild();
    }
    void removeRow(int at)
    {
        rows_.removeAt(at);
        rebuild();
    }
    int     knownCount() const { return rows_.size(); }
    int     itemCount() const { return logicalCount_; }
    int     logicalIndex(int knownIndex) const;
    int     knownIndex(int logicalIndex) const;
    int     indexOfKey(const QString &) const;
    QString keyAt(int logicalIndex) const;
    bool    isAvailable(int logicalIndex) const { return knownIndex(logicalIndex) >= 0; }
    bool    isVirtual() const { return virtual_; }
    bool    canSeek() const { return count_ >= 0 && !indexRejected_; }
    qint64  estimatedTotalMessages() const { return estimatedTotal_; }
    qreal   visibleRatio() const;
    int     archiveCount() const { return count_; }

    // UID order comes from the archive, including non-rendered events. Empty
    // row keys mean protocol-only events. Aliases may name one corrected row.
    void    observePage(const QStringList &uids, const QStringList &keys, int count, int firstIndex,
                        const QString &before = {}, const QString &after = {}, bool tailChain = false, bool stable = true);
    void    restoreSnapshot(int count, const QString &lastUid);
    void    enableVirtual();
    void    rejectIndex();
    void    rebuild();
    void    clear();
    Request requestForRange(int first, int last, int limit, quint64 generation, bool preferIndex = false) const;
    int     rankOfUid(const QString &uid) const { return uidRanks_.value(uid, -1); }
    QString closestKnownKey(int eventIndex) const;
    int     logicalPositionForEvent(int eventIndex) const;
    void    beginNavigation(const Request &);
    Request continueNavigation(const Request &, bool queryComplete, bool wantMore = false);
    int     navigationTarget() const { return navigationTarget_; }
    bool    lastPageHasMessage() const { return lastPageHasMessage_; }
    bool    lastPageWasIndexed() const { return lastPageServerReported_; }
    const QSet<QString>   &lastPageMessageKeys() const { return lastPageMessageKeys_; }
    int                    lastPageFirstIndex() const { return lastPageFirstIndex_; }
    bool                   navigationStalled() const { return navigationStalled_; }
    const QVector<Window> &windows() const { return windows_; }

private:
    friend class MamChatWidget;
    QList<Row>              rows_;
    QHash<QString, int>     uidRanks_;
    QMap<int, QString>      rankUids_;
    QHash<QString, int>     keyRanks_;
    QHash<QString, QString> keyUids_;
    qint64                  sampledEventCount_ = 0;
    QSet<QString>           sampledRows_;
    QVector<Window>         windows_;
    QVector<int>            positions_;
    QHash<int, int>         knownAt_;
    QHash<QString, int>     indexes_;
    int                     count_ = -1, logicalCount_ = 0, tailFirst_ = -1;
    qint64                  estimatedTotal_ = 0;
    bool                    virtual_ = false, indexRejected_ = false;
    QStringList             lastPageUids_;
    QSet<QString>           lastPageMessageKeys_;
    int                     lastPageFirstIndex_ = -1;
    bool                    lastPageHasMessage_ = false, lastPageServerReported_ = false;
    int                     navigationTarget_   = -1;
    bool                    navigationWasIndex_ = false, navigationStalled_ = false;
    QString                 navigationRightCursor_;
    QSet<QString>           navigationCursors_;
};

Q_DECLARE_METATYPE(MamHistorySource::Request)
