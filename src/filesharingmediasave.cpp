/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingmediasave.h"
#include "filedownload.h"
#include "filereader.h"
#include "qitemedia.h"
#include <QFile>
#include <QTimer>
#include <algorithm>
#include <utility>

FileSharingMediaSave::FileSharingMediaSave(QIODevice *input, std::optional<quint64> size, bool waitForValidation,
                                           const QString &path, ITEMediaState *state, QObject *parent,
                                           Callback onFinished, ProgressCallback onProgress) :
    QObject(parent), onFinished_(std::move(onFinished)), onProgress_(std::move(onProgress)), output_(path),
    state_(state), size_(size), accepted_(!waitForValidation)
{
    if (input)
        attachInput(input);
    state_->setDownload(true, 0);
    // Start after construction, so synchronous failure cannot run a partially
    // constructed job, and dialogs/callers can return before progress signals.
    QTimer::singleShot(0, this, [this] {
        if (!output_.open(QIODevice::WriteOnly)) {
            finish(output_.errorString());
            return;
        }
        if (open_) {
            const QPointer<FileSharingMediaSave> guard(this);
            cancelOpen_ = open_(this, [guard](QIODevice *input, quint64 size, QString error) {
                if (!guard || guard->stopped_)
                    return;
                if (!error.isEmpty()) {
                    guard->finish(error);
                    return;
                }
                guard->size_ = size;
                guard->attachInput(input);
                guard->openInput();
            });
        } else if (download_) {
            const QPointer<FileSharingMediaSave> guard(this);
            goal_ = download_->downloadAll(
                [guard](const QString &path, MediaChunkError error) {
                    if (!guard || guard->stopped_)
                        return;
                    guard->goal_ = 0;
                    if (!error.isEmpty()) {
                        guard->finish(error.message());
                        return;
                    }
                    guard->attachInput(new FileReader(path));
                    guard->openInput();
                },
                [guard](qreal progress) {
                    if (!guard || guard->stopped_)
                        return;
                    guard->reportProgress(progress);
                });
            if (!goal_)
                finish(tr("Cannot start file download"));
        } else {
            openInput();
        }
    });
}

FileSharingMediaSave::FileSharingMediaSave(FileDownload *download, const QString &path, ITEMediaState *state,
                                           QObject *parent, Callback completed, ProgressCallback progress) :
    FileSharingMediaSave(static_cast<QIODevice *>(nullptr), download->size(), false, path, state, parent,
                         std::move(completed), std::move(progress))
{
    download_ = download;
    download_->pin(true);
    downloadPinned_ = true;
    connect(download, &QObject::destroyed, this, [this] { fail(tr("Download source disappeared")); });
}
FileSharingMediaSave::FileSharingMediaSave(Open open, const QString &path, ITEMediaState *state, QObject *parent,
                                           Callback completed, ProgressCallback progress) :
    FileSharingMediaSave(static_cast<QIODevice *>(nullptr), {}, false, path, state, parent, std::move(completed),
                         std::move(progress))
{
    open_ = std::move(open);
}

FileSharingMediaSave::~FileSharingMediaSave()
{
    stopped_ = true;
    releaseGoal();
}

void FileSharingMediaSave::releaseGoal()
{
    if (auto cancel = std::exchange(cancelOpen_, {}))
        cancel();
    if (!download_)
        return;
    if (auto goal = std::exchange(goal_, 0))
        download_->cancelGoal(goal);
    if (std::exchange(downloadPinned_, false))
        download_->pin(false);
}

void FileSharingMediaSave::attachInput(QIODevice *input)
{
    input_ = input;
    input->setParent(this);
    connect(input, &QIODevice::readyRead, this, [this] { schedule(); });
    connect(input, &QIODevice::aboutToClose, this, &FileSharingMediaSave::sourceClosed);
    connect(input, &QObject::destroyed, this, [this] { fail(tr("Download source disappeared")); });
    if (auto reader = qobject_cast<FileReader *>(input))
        connect(reader, &FileReader::failed, this, [this, reader] { fail(reader->errorString()); });
}

void FileSharingMediaSave::openInput()
{
    if (!input_ || (!input_->isOpen() && !input_->open(QIODevice::ReadOnly))) {
        finish(input_ ? input_->errorString() : tr("Download source disappeared"));
        return;
    }
    schedule();
}

void FileSharingMediaSave::validated()
{
    accepted_ = true;
    schedule();
}

void FileSharingMediaSave::sourceClosed()
{
    closed_ = true;
    schedule();
}

void FileSharingMediaSave::fail(const QString &error)
{
    failure_ = error.isEmpty() ? tr("Download failed") : error;
    schedule();
}

void FileSharingMediaSave::cancel() { finish(tr("Download cancelled")); }

void FileSharingMediaSave::schedule()
{
    if (stopped_ || pending_)
        return;
    pending_ = true;
    QTimer::singleShot(0, this, [this] {
        pending_ = false;
        pump();
    });
}

void FileSharingMediaSave::pump()
{
    if (stopped_)
        return;
    if (!failure_.isEmpty()) {
        finish(failure_);
        return;
    }
    if (!input_)
        return; // The full-download goal is still filling the cache.
    const QPointer<FileSharingMediaSave> guard(this);
    int                                  budget = 256 * 1024;
    while (budget > 0 && input_->bytesAvailable() > 0) {
        const auto bytes = input_->read(qMin(budget, 64 * 1024));
        if (!guard || stopped_)
            return;
        if (!failure_.isEmpty()) {
            finish(failure_);
            return;
        }
        if (size_ && (copied_ > *size_ || quint64(bytes.size()) > *size_ - copied_)) {
            finish(tr("Unexpected media size"));
            return;
        }
        if (bytes.isEmpty()) {
            finish(tr("Cannot read media data"));
            return;
        }
        if (output_.write(bytes) != bytes.size()) {
            finish(output_.errorString());
            return;
        }
        copied_ += quint64(bytes.size());
        budget -= bytes.size();
    }
    if ((accepted_ || closed_) && input_->bytesAvailable() == 0) {
        if (!accepted_)
            finish(tr("Media download ended before validation"));
        else if (size_ && copied_ != *size_)
            finish(tr("Incomplete media download"));
        else if (!output_.commit())
            finish(output_.errorString());
        else
            finish();
        return;
    }
    if (input_->bytesAvailable() > 0)
        schedule();
    reportProgress(size_ && *size_ ? qreal(copied_) / *size_ : -1);
}

void FileSharingMediaSave::reportProgress(qreal progress)
{
    if (progress >= 0) {
        progress      = std::max(progress, lastProgress_);
        lastProgress_ = progress;
    }
    const QPointer<FileSharingMediaSave> guard(this);
    const auto                           notify = onProgress_;
    if (state_)
        state_->setDownload(true, progress);
    if (guard && !stopped_ && notify)
        notify(progress);
}

void FileSharingMediaSave::finish(const QString &error)
{
    if (stopped_)
        return;
    stopped_ = true;
    releaseGoal();
    if (!error.isEmpty()) {
        output_.cancelWriting();
    }
    const auto state     = state_;
    const auto completed = std::exchange(onFinished_, {});
    const auto path      = error.isEmpty() ? output_.fileName() : QString();
    deleteLater();
    if (state)
        state->setDownload(false, error.isEmpty() ? 1 : -1, error);
    if (completed)
        completed(path, error);
}
