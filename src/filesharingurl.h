#pragma once

#include <QUrl>
#include <QUrlQuery>

namespace FileSharingUrl {
enum class Mode { Download, Stream };

inline Mode mode(const QUrl &url)
{
    const auto values = QUrlQuery(url).allQueryItemValues(QStringLiteral("mode"));
    // Unmarked links retain ordinary download semantics. Ambiguous URLs must
    // not accidentally turn a sequential transfer into chunked playback.
    return values.size() == 1 && values.first() == QLatin1String("stream") ? Mode::Stream : Mode::Download;
}

inline QUrl withMode(QUrl url, Mode mode)
{
    QUrlQuery query(url);
    query.removeAllQueryItems(QStringLiteral("mode"));
    query.addQueryItem(QStringLiteral("mode"),
                       mode == Mode::Stream ? QStringLiteral("stream") : QStringLiteral("download"));
    url.setQuery(query);
    return url;
}
}
