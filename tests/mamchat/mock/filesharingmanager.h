/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
// Only the storage/network boundary is substituted. PsiTextView's <share>
// parser, text objects and qite audio/video controllers are real.
#include "filesharingdownloader.h"
#include "qite.h"
#include <QHash>
#include <QImage>
#include <QUrl>
#include <optional>
#include <functional>

class FileSharingItem : public QObject {
    Q_OBJECT
public:
    explicit FileSharingItem(const QString &mime, QObject *parent = nullptr) : QObject(parent), mime_(mime) { }
    QString                mimeType() const { return mime_; }
    QString                displayName() const { return QStringLiteral("test.7z"); }
    QString                fileName() const { return cachedPath_; }
    bool                   isCached() const { return true; }
    QImage                 preview(const QSize &) const { return preview_; }
    std::optional<quint64> fileSize() const { return 100; }
    QUrl                   simpleSource() const { return source_; }
    QUrl                   source_ = QUrl(QStringLiteral("https://example.org/file"));
    FileShareDownloader   *download() { return new FileShareDownloader(this); }
    QImage                 preview_;
    QString                cachedPath_;
signals:
    void downloadFinished();

private:
    QString mime_;
};
class FileSharingDeviceOpener : public ITEMediaOpener {
public:
    QHash<QString, FileSharingItem *>         items;
    QHash<QString, FileSharingDeviceOpener *> accounts;
    int                                       opens = 0, saves = 0;
    FileSharingDeviceOpener                  *forAccount(const QString &id) const
    {
        return id.isEmpty() ? const_cast<FileSharingDeviceOpener *>(this) : accounts.value(id);
    }
    FileSharingItem *sharedItem(const QString &id) { return items.value(id); }
    using SaveCallback = std::function<void(const QString &, const QString &)>;
    using ProgressCallback = std::function<void(qreal)>;
    bool             saveAs(const QUrl &, QWidget *, const SaveCallback & = {}, const ProgressCallback & = {})
    {
        ++saves;
        return true;
    }
    bool             saveTo(const QUrl &, const QString &, const SaveCallback & = {}, const ProgressCallback & = {})
    {
        ++saves;
        return true;
    }
    void             cancelSave(const QUrl &) { }
    QIODevice       *open(QUrl &) override
    {
        ++opens;
        return nullptr;
    }
    void     close(QIODevice *) override { }
    QVariant metadata(const QUrl &) override { return {}; }
};
