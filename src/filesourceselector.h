#pragma once
#include <QHash>
#include <QStringList>
#include <algorithm>

// Owner-thread policy. Probe sources, then prefer estimated completion time.
class FileSourceSelector {
public:
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
