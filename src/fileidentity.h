/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>

// Persisted range identity shared by history references and range storage.
// Keep the stream version and encoding stable across Qt versions.
namespace FileIdentity {
inline QString source(const QString &account, const QString &hash) { return account + QChar(0) + hash; }
inline QString rangeKey(const QString &source, quint64 size)
{
    QByteArray  identity;
    QDataStream stream(&identity, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_10);
    stream << source << size;
    return QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
}
inline QString rangeKey(const QString &account, const QString &hash, quint64 size)
{
    return rangeKey(source(account, hash), size);
}
}
