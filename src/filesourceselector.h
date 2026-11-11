#pragma once
#include "aesgcmurl.h"
#include <QHash>
#include <QMultiMap>
#include <QStringList>
#include <QUrl>
#include <algorithm>

// Owner-thread policy. Probe sources, then prefer estimated completion time.
class FileSourceSelector {
public:
    enum class Type { None, BOB, FTP, Jingle, HTTP }; // lowest priority first

    static Type type(const QString &uri)
    {
        const auto scheme = QUrl(uri).scheme();
        if (scheme == QLatin1String("aesgcm") && AesGcmUrl::parse(QUrl(uri)))
            return Type::HTTP;
        if (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
            return Type::HTTP;
        if (scheme == QLatin1String("xmpp"))
            return Type::Jingle;
        if (scheme == QLatin1String("ftp"))
            return Type::FTP;
        if (scheme == QLatin1String("cid"))
            return Type::BOB;
        return Type::None;
    }

    // Consume from the end: preferred transports first, preserving the
    // sender's order within each transport. Unsupported schemes are omitted.
    static QStringList prioritized(const QStringList &uris)
    {
        QMultiMap<int, QString> sorted;
        for (const auto &uri : uris) {
            const auto sourceType = type(uri);
            if (sourceType != Type::None)
                sorted.insert(int(sourceType), uri);
        }
        return sorted.values();
    }

    QString acquire(const QStringList &uris)
    {
        QString best;
        auto    cost = [this](const QString &uri) {
            if (!rates_.contains(uri) && !active_.value(uri))
                return 0.0;
            return (active_.value(uri) + 1.0) / rates_.value(uri, 1.0);
        };
        for (const auto &uri : uris)
            if (best.isEmpty() || cost(uri) < cost(best))
                best = uri;
        if (!best.isEmpty())
            ++active_[best];
        return best;
    }
    void release(const QString &uri, quint64 bytes, qint64 milliseconds, bool success, bool cancelled)
    {
        active_[uri] = std::max(0, active_.value(uri) - 1);
        if (success)
            rates_[uri] = std::max(0.001, double(bytes) / std::max<qint64>(1, milliseconds));
        else if (!cancelled)
            rates_[uri] = 0.001;
    }

private:
    QHash<QString, int>    active_;
    QHash<QString, double> rates_;
};
