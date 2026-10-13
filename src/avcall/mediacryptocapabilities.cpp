// SPDX-License-Identifier: GPL-2.0-or-later
#include "mediacryptocapabilities.h"

#include <QCoreApplication>
#include <QtConcurrentRun>

MediaCryptoCapabilities::MediaCryptoCapabilities(QObject *parent) : QObject(parent)
{
    connect(&watcher_, &QFutureWatcher<QStringList>::finished, this, [this] {
        profiles_ = watcher_.result();
        complete_ = true;
        emit changed();
    });
    // Drain before QCA/plugin shutdown, even when this object belongs to qApp
    // and therefore would otherwise be destroyed after QCA::Initializer.
    if (qApp)
        connect(qApp, &QCoreApplication::aboutToQuit, this, &MediaCryptoCapabilities::waitForFinished);
}

MediaCryptoCapabilities::~MediaCryptoCapabilities() { waitForFinished(); }

void MediaCryptoCapabilities::start(const std::function<QStringList()> &probe)
{
    if (started_)
        return;
    started_ = true;
    watcher_.setFuture(QtConcurrent::run(probe));
}

void MediaCryptoCapabilities::waitForFinished()
{
    if (started_)
        watcher_.waitForFinished();
}
