/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QObject>
#include <QPointer>
#include <QSaveFile>
#include <optional>
#include <functional>

class ITEMediaState;

// A bounded, queued copy. Transport callbacks only schedule work: in particular
// finishing a read must never delete/abort the stream inside its read hook.
class FileSharingMediaSave : public QObject {
public:
    using Callback = std::function<void(const QString &path, const QString &error)>;
    using ProgressCallback = std::function<void(qreal)>;
    // Takes ownership and opens asynchronously, so validation/failure handlers
    // can be connected before the first read.
    FileSharingMediaSave(QIODevice *input, std::optional<quint64> size, bool waitForValidation, const QString &path,
                         ITEMediaState *state, QObject *parent, Callback onFinished = {},
                         ProgressCallback onProgress = {});
    void validated();
    void sourceClosed();
    void fail(const QString &error);
    void cancel();

private:
    void                    schedule();
    void                    pump();
    void                    finish(const QString &error = {});
    Callback                onFinished_;
    ProgressCallback        onProgress_;
    QSaveFile               output_;
    QPointer<QIODevice>     input_;
    QPointer<ITEMediaState> state_;
    std::optional<quint64>  size_;
    QString                 failure_;
    quint64                 copied_  = 0;
    bool                    pending_ = false, stopped_ = false, accepted_ = false, closed_ = false;
};
