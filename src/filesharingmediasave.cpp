/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingmediasave.h"
#include "qitemedia.h"
#include <QFile>
#include <QTimer>

FileSharingMediaSave::FileSharingMediaSave(QIODevice *input, std::optional<quint64> size, bool waitForValidation,
                                           const QString &path, ITEMediaState *state, QObject *parent) :
    QObject(parent), output_(path), input_(input), state_(state), size_(size), accepted_(!waitForValidation)
{
    input->setParent(this);
    connect(input, &QIODevice::readyRead, this, [this] { schedule(); });
    connect(input, &QIODevice::aboutToClose, this, &FileSharingMediaSave::sourceClosed);
    connect(input, &QObject::destroyed, this, [this] { fail(tr("Download source disappeared")); });
    state_->setDownload(true, 0);
    // Start after construction, so synchronous failure cannot run a partially
    // constructed job, and dialogs/callers can return before progress signals.
    QTimer::singleShot(0, this, [this] {
        if (!output_.open(QIODevice::WriteOnly)) {
            finish(output_.errorString());
            return;
        }
        if (!input_ || (!input_->isOpen() && !input_->open(QIODevice::ReadOnly))) {
            finish(input_ ? input_->errorString() : tr("Download source disappeared"));
            return;
        }
        schedule();
    });
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
    if (state_)
        state_->setDownload(true, size_ && *size_ ? qreal(copied_) / *size_ : -1);
}

void FileSharingMediaSave::finish(const QString &error)
{
    if (stopped_)
        return;
    stopped_ = true;
    if (!error.isEmpty()) {
        output_.cancelWriting();
    }
    deleteLater();
    if (state_)
        state_->setDownload(false, error.isEmpty() ? 1 : -1, error);
}
