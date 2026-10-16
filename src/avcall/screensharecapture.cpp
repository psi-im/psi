// SPDX-License-Identifier: GPL-2.0-or-later
#include "screensharecapture.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

ScreenShareCapture::ScreenShareCapture(QObject *parent) : QObject(parent), backend_(this)
{
    connect(&backend_, &PsiMedia::ScreenCapture::sourcesAvailable, this, &ScreenShareCapture::showSources);
    connect(&backend_, &PsiMedia::ScreenCapture::ready, this, [this](const QString &id, const QString &label) {
        if (!selecting_)
            return;
        QPointer<ScreenShareCapture> guard(this);
        if (picker_) {
            auto *dialog = picker_.data();
            picker_.clear();
            dialog->disconnect(this);
            dialog->accept();
        }
        if (!guard)
            return;
        selecting_            = false;
        active_               = true;
        source_               = id;
        const auto generation = generation_;
        emit       selectingChanged(false);
        if (guard && active_ && generation == generation_)
            emit ready(id, label);
    });
    connect(&backend_, &PsiMedia::ScreenCapture::stopped, this, [this] { finish(); });
    connect(&backend_, &PsiMedia::ScreenCapture::failed, this, [this](const QString &error) {
        QPointer<ScreenShareCapture> guard(this);
        finish();
        if (guard)
            emit failed(error);
    });
}
ScreenShareCapture::~ScreenShareCapture() { stop(); }
bool ScreenShareCapture::available() { return PsiMedia::ScreenCapture::available(); }
bool ScreenShareCapture::selecting() const { return selecting_; }
void ScreenShareCapture::select(QWidget *parent)
{
    QPointer<ScreenShareCapture> guard(this);
    const auto                   previousGeneration = generation_;
    stop();
    if (!guard || generation_ != previousGeneration + 1)
        return;
    selecting_            = true;
    parent_               = parent;
    const auto generation = generation_;
    emit       selectingChanged(true);
    if (guard && generation == generation_ && selecting_)
        backend_.startSelection();
}
void ScreenShareCapture::stop()
{
    QPointer<ScreenShareCapture> guard(this);
    const auto                   generation = generation_;
    // The provider emits stopped() before releasing permission/resources, so
    // synchronous consumers quiesce capture while the old lease is still valid.
    backend_.stop();
    if (guard && generation == generation_)
        finish();
}
void ScreenShareCapture::finish()
{
    ++generation_;
    const bool wasActive    = std::exchange(active_, false);
    const bool wasSelecting = std::exchange(selecting_, false);
    source_.clear();
    QPointer<ScreenShareCapture> guard(this);
    if (picker_) {
        auto *dialog = picker_.data();
        picker_.clear();
        dialog->disconnect(this);
        dialog->reject();
    }
    if (!guard)
        return;
    // Revoke capture synchronously before releasing the provider's permission.
    if (wasActive)
        emit stopped();
    if (guard && wasSelecting && !selecting_)
        emit selectingChanged(false);
}
void ScreenShareCapture::showSources()
{
    if (!selecting_ || picker_)
        return;
    auto *dialog = new QDialog(parent_);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Choose what to share"));
    dialog->resize(760, 510);
    picker_      = dialog;
    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(new QLabel(tr("Share a display or a single window"), dialog));
    auto *tabs = new QTabWidget(dialog);
    layout->addWidget(tabs, 1);
    auto      *displays = new QListWidget(tabs);
    auto      *windows  = new QListWidget(tabs);
    const auto sources  = backend_.sources();
    const bool previews
        = std::any_of(sources.cbegin(), sources.cend(), [](const auto &source) { return !source.thumbnail.isEmpty(); });
    for (auto *list : { displays, windows }) {
        if (previews) {
            list->setViewMode(QListView::IconMode);
            list->setIconSize(QSize(210, 125));
            list->setGridSize(QSize(230, 165));
            list->setResizeMode(QListView::Adjust);
            list->setMovement(QListView::Static);
            list->setSpacing(8);
        }
    }
    tabs->addTab(displays, tr("Displays"));
    tabs->addTab(windows, tr("Windows"));
    for (const auto &source : sources) {
        auto   *list = source.kind == PsiMedia::ScreenCapture::Source::Display ? displays : windows;
        QPixmap preview;
        preview.loadFromData(source.thumbnail, "PNG");
        auto *item = new QListWidgetItem(QIcon(preview), source.label, list);
        item->setData(Qt::UserRole, source.id);
        item->setToolTip(source.label.toHtmlEscaped());
    }
    tabs->setTabEnabled(1, windows->count() > 0);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Share"));
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    layout->addWidget(buttons);
    auto update = [tabs, buttons] {
        auto *list = qobject_cast<QListWidget *>(tabs->currentWidget());
        buttons->button(QDialogButtonBox::Ok)->setEnabled(list && list->currentItem());
    };
    for (auto *list : { displays, windows }) {
        connect(list, &QListWidget::itemSelectionChanged, dialog, update);
        connect(list, &QListWidget::itemDoubleClicked, dialog, [dialog] { dialog->accept(); });
    }
    connect(tabs, &QTabWidget::currentChanged, dialog, update);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    const auto generation = generation_;
    connect(dialog, &QDialog::accepted, this, [this, tabs, generation] {
        if (generation != generation_)
            return;
        auto *list = qobject_cast<QListWidget *>(tabs->currentWidget());
        auto *item = list ? list->currentItem() : nullptr;
        if (item) {
            picker_.clear(); // this dialog is already accepting
            backend_.choose(item->data(Qt::UserRole).toString());
        } else
            stop();
    });
    connect(dialog, &QDialog::rejected, this, [this, generation] {
        if (generation == generation_) {
            picker_.clear(); // this dialog is already rejecting
            stop();
        }
    });
    dialog->show();
}
