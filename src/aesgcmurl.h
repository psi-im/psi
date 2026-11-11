/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QByteArray>
#include <QUrl>
#include <optional>

// XEP-0454: HTTPS transport, with a 12-byte IV followed by a 32-byte key
// in the fragment. The fragment is local key material, never an HTTP field.
struct AesGcmUrl {
    QUrl                            transport;
    QByteArray                      iv, key;
    static std::optional<AesGcmUrl> parse(const QUrl &url)
    {
        if (!url.isValid() || url.scheme() != QLatin1String("aesgcm") || url.host().isEmpty()
            || !url.userInfo().isEmpty())
            return {};
        const auto fragment = url.fragment(QUrl::FullyEncoded).toLatin1();
        if (fragment.size() != 88)
            return {};
        for (const auto c : fragment)
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                return {};
        const auto bytes     = QByteArray::fromHex(fragment);
        auto       transport = url;
        transport.setScheme("https");
        transport.setFragment({});
        return AesGcmUrl { transport, bytes.left(12), bytes.mid(12) };
    }
};
