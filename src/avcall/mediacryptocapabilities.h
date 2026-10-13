// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef MEDIACRYPTOCAPABILITIES_H
#define MEDIACRYPTOCAPABILITIES_H

#include <QFutureWatcher>
#include <QObject>
#include <QStringList>
#include <functional>

// Runs a value-only crypto probe. Provider QObjects and live media/device state
// stay on their owning threads; readers receive only the completed snapshot.
class MediaCryptoCapabilities : public QObject {
    Q_OBJECT
public:
    explicit MediaCryptoCapabilities(QObject *parent = nullptr);
    ~MediaCryptoCapabilities() override;
    void        start(const std::function<QStringList()> &probe);
    void        waitForFinished();
    bool        complete() const { return complete_; }
    QStringList profiles() const { return profiles_; }

signals:
    void changed();

private:
    QFutureWatcher<QStringList> watcher_;
    QStringList                 profiles_;
    bool                        started_  = false;
    bool                        complete_ = false;
};

#endif
