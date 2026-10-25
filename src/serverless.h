// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QHostAddress>
#include <QNetworkInterface>
#include <QString>

namespace Serverless {
// A missing explicitly selected interface is an error, never a silent fallback
// to another network. Automatic selection excludes loopback and point-to-point
// links, and prefers IPv4 on a running multicast-capable interface.
QHostAddress addressForInterface(const QString &name);
QHostAddress addressForInterface(const QNetworkInterface &interface);
QString      defaultIdentity(const QString &accountId);
QString      defaultName();
}
