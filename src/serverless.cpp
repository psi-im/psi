// SPDX-License-Identifier: GPL-2.0-or-later
#include "serverless.h"
#include <QHostInfo>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>

namespace Serverless {
QHostAddress addressForInterface(const QNetworkInterface &interface)
{
    const auto flags = interface.flags();
    if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
        || (!flags.testFlag(QNetworkInterface::CanMulticast) && !flags.testFlag(QNetworkInterface::IsLoopBack)))
        return {};
    QHostAddress ipv6;
    for (const auto &entry : interface.addressEntries()) {
        const auto address = entry.ip();
        if (address.isNull() || address.isMulticast())
            continue;
        if (address.protocol() == QAbstractSocket::IPv4Protocol)
            return address;
        if (address.protocol() == QAbstractSocket::IPv6Protocol && ipv6.isNull()) {
            ipv6             = address;
            const auto bytes = ipv6.toIPv6Address();
            if (bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80)
                ipv6.setScopeId(interface.name());
        }
    }
    return ipv6;
}
QHostAddress addressForInterface(const QString &name)
{
    if (!name.isEmpty())
        return addressForInterface(QNetworkInterface::interfaceFromName(name));
    QHostAddress ipv6;
    auto         interfaces = QNetworkInterface::allInterfaces();
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    std::stable_sort(interfaces.begin(), interfaces.end(), [](const auto &a, const auto &b) {
        const auto physical = [](const auto &interface) {
            return interface.type() == QNetworkInterface::Ethernet || interface.type() == QNetworkInterface::Wifi;
        };
        return physical(a) && !physical(b);
    });
#endif
    for (const auto &interface : interfaces) {
        if (interface.flags().testFlag(QNetworkInterface::IsLoopBack)
            || interface.flags().testFlag(QNetworkInterface::IsPointToPoint))
            continue;
        const auto address = addressForInterface(interface);
        if (address.protocol() == QAbstractSocket::IPv4Protocol)
            return address;
        if (ipv6.isNull())
            ipv6 = address;
    }
    return ipv6;
}
QString defaultName()
{
    auto name = qEnvironmentVariable("USER");
    if (name.isEmpty())
        name = qEnvironmentVariable("USERNAME");
    return name.isEmpty() ? QStringLiteral("Psi") : name;
}
QString defaultIdentity(const QString &accountId)
{
    const QRegularExpression invalid(QStringLiteral("[^a-z0-9-]"));
    auto host = QString::fromLatin1(QUrl::toAce(QHostInfo::localHostName().section('.', 0, 0))).toLower();
    host.replace(invalid, QStringLiteral("-"));
    host = host.left(24);
    if (host.isEmpty() || host.front() == '-' || host.back() == '-')
        host = QStringLiteral("computer");
    auto user = defaultName().toLower();
    user.replace(invalid, QStringLiteral("-"));
    if (user.isEmpty())
        user = QStringLiteral("psi-") + accountId.mid(1, 8);
    return user.left(20) + '@' + host;
}
}
