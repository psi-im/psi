// SPDX-License-Identifier: GPL-2.0-or-later
#include "callview.h"
#include "../psimedia/psimedia.h"
#include "screensharecapture.h"
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
QIcon callIcon(int kind)
{
    QPixmap image(48, 48);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    painter.setPen(QPen(QColor("#e6eef9"), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (kind == 0) {
        painter.drawRoundedRect(QRectF(9, 3, 6, 11), 3, 3);
        painter.drawArc(QRectF(6, 6, 12, 12), 180 * 16, 180 * 16);
        painter.drawLine(QPointF(12, 18), QPointF(12, 22));
        painter.drawLine(QPointF(8, 22), QPointF(16, 22));
    } else if (kind == 1) {
        painter.drawRoundedRect(QRectF(3, 6, 12, 12), 2, 2);
        QPainterPath lens;
        lens.moveTo(15, 10);
        lens.lineTo(21, 7);
        lens.lineTo(21, 17);
        lens.lineTo(15, 14);
        painter.drawPath(lens);
    } else if (kind == 2) {
        painter.drawRoundedRect(QRectF(2, 3, 20, 14), 2, 2);
        painter.drawLine(QPointF(12, 17), QPointF(12, 21));
        painter.drawLine(QPointF(7, 21), QPointF(17, 21));
        painter.drawLine(QPointF(12, 14), QPointF(12, 7));
        painter.drawLine(QPointF(12, 7), QPointF(9, 10));
        painter.drawLine(QPointF(12, 7), QPointF(15, 10));
    } else {
        QPainterPath receiver;
        receiver.moveTo(3, 14);
        receiver.cubicTo(6, 6, 18, 6, 21, 14);
        painter.drawPath(receiver);
        painter.drawLine(QPointF(3, 14), QPointF(7, 16));
        painter.drawLine(QPointF(21, 14), QPointF(17, 16));
    }
    return QIcon(image);
}
}

CallView::CallView(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("callView"));
    setStyleSheet(QStringLiteral(
        "QWidget#callView { background: #111827; color: #f3f4f6; }"
        "QLabel { color: #f3f4f6; background: transparent; }"
        "QLineEdit { color: #f3f4f6; background: transparent; border: none; font-size: 20px; font-weight: 600; "
        "padding: 4px; }"
        "QLabel#callStatus, QLabel#callDuration { color: #9ca3af; }"
        "QFrame#callStage { background: #1b2435; border: 1px solid #2b3548; border-radius: 18px; }"
        "QFrame#sharingBar { background: #153b37; border: 1px solid #2dd4bf; border-radius: 10px; }"
        "QPushButton { color: #f3f4f6; background: #293447; border: 1px solid #39465c; border-radius: 12px; padding: "
        "12px 18px; font-weight: 600; }"
        "QPushButton:hover { background: #36445c; }"
        "QPushButton:focus { border: 2px solid #93c5fd; }"
        "QPushButton:disabled { color: #718096; background: #202a3a; border-color: #293447; }"
        "QPushButton:checked { background: #145c55; border-color: #2dd4bf; }"
        "QPushButton#callAccept { background: #0f766e; border-color: #2dd4bf; }"
        "QPushButton#callEnd { background: #be354b; border-color: #e35b70; }"
        "QPushButton#callEnd:hover { background: #d4435a; }"
        "QComboBox { color: #d1d5db; background: #293447; border: 1px solid #39465c; border-radius: 8px; padding: 8px; "
        "}"
        "QComboBox QAbstractItemView { color: #f3f4f6; background: #293447; selection-background-color: #0f766e; }"));
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(18);
    auto header   = new QHBoxLayout;
    auto identity = new QVBoxLayout;
    peer_         = new QLineEdit(this);
    peer_->setObjectName(QStringLiteral("callPeer"));
    peer_->setPlaceholderText(tr("Contact or address"));
    status_ = new QLabel(tr("Ready to call"), this);
    status_->setTextFormat(Qt::PlainText);
    status_->setObjectName(QStringLiteral("callStatus"));
    identity->addWidget(peer_);
    identity->addWidget(status_);
    header->addLayout(identity, 1);
    duration_ = new QLabel(this);
    duration_->setObjectName(QStringLiteral("callDuration"));
    header->addWidget(duration_);
    layout->addLayout(header);

    auto stage = new QFrame(this);
    stage->setObjectName(QStringLiteral("callStage"));
    stage->setMinimumSize(420, 260);
    auto stageLayout = new QGridLayout(stage);
    stageLayout->setSpacing(12);
    stageLayout->setColumnStretch(0, 4);
    stageLayout->setColumnStretch(1, 1);
    stageLayout->setContentsMargins(12, 12, 12, 12);
    placeholder_ = new QLabel(tr("Audio call\nTurn on your camera or share your screen"), stage);
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setStyleSheet(QStringLiteral("color: #aab6c8; font-size: 18px;"));
    stageLayout->addWidget(placeholder_, 0, 0, 1, 2);
    video_ = new PsiMedia::VideoWidget(stage);
    video_->setMinimumSize(160, 120);
    video_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    presentation_ = new PsiMedia::VideoWidget(stage);
    presentation_->setMinimumSize(320, 180);
    presentation_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    for (auto video : { video_, presentation_ }) {
        video->hide();

        connect(video, &PsiMedia::VideoWidget::videoSizeChanged, this, [this, video] {
            if (!active_)
                return;
            video->show();
            placeholder_->hide();
        });
    }
    stageLayout->addWidget(presentation_, 0, 0);
    stageLayout->addWidget(video_, 0, 1);
    layout->addWidget(stage, 1);

    sharingBar_ = new QFrame(this);
    sharingBar_->setObjectName(QStringLiteral("sharingBar"));
    auto sharingLayout = new QHBoxLayout(sharingBar_);
    sharingLabel_      = new QLabel(sharingBar_);
    sharingLabel_->setWordWrap(true);
    sharingLabel_->setTextFormat(Qt::PlainText);
    sharingLayout->addWidget(sharingLabel_, 1);
    auto stop = new QPushButton(tr("Stop sharing"), sharingBar_);
    connect(stop, &QPushButton::clicked, this, &CallView::shareRequested);
    sharingLayout->addWidget(stop);
    sharingBar_->hide();
    layout->addWidget(sharingBar_);

    auto controls = new QHBoxLayout;
    controls->setSpacing(10);
    microphone_ = new QPushButton(tr("Microphone"), this);
    microphone_->setObjectName(QStringLiteral("callMicrophone"));
    microphone_->setCheckable(true);
    microphone_->setChecked(true);
    microphone_->setToolTip(tr("Turn your microphone on or off"));
    connect(microphone_, &QPushButton::toggled, this, [this](bool enabled) {
        microphone_->setText(enabled ? tr("Microphone") : tr("Muted"));
        emit microphoneChanged(enabled);
    });
    camera_ = new QPushButton(tr("Camera off"), this);
    camera_->setObjectName(QStringLiteral("callCamera"));
    camera_->setCheckable(true);
    camera_->setToolTip(tr("Turn your camera on or off"));
    connect(camera_, &QPushButton::toggled, this, [this](bool enabled) {
        camera_->setText(enabled ? tr("Camera on") : tr("Camera off"));
        emit cameraChanged(enabled);
    });
    share_ = new QPushButton(tr("Share screen"), this);
    share_->setObjectName(QStringLiteral("callShare"));
    share_->setCheckable(true);
    share_->setEnabled(false);
    share_->setToolTip(tr("Share a display or an individual window"));
    connect(share_, &QPushButton::clicked, this, &CallView::shareRequested);
    microphone_->setIcon(callIcon(0));
    camera_->setIcon(callIcon(1));
    share_->setIcon(callIcon(2));
    for (auto button : { microphone_, camera_, share_ })
        button->setIconSize(QSize(22, 22));
    quality_ = new QComboBox(this);
    quality_->addItem(tr("HD · 2 Mbps"), 2000);
    quality_->addItem(tr("Balanced · 1 Mbps"), 1000);
    quality_->addItem(tr("Low bandwidth · 400 kbps"), 400);
    quality_->setCurrentIndex(1);
    quality_->setAccessibleName(tr("Video quality"));
    controls->addWidget(microphone_);
    controls->addWidget(camera_);
    controls->addWidget(share_);
    controls->addStretch();
    layout->addLayout(controls);
    auto actions = new QHBoxLayout;
    actions->addWidget(quality_);
    actions->addStretch();
    accept_ = new QPushButton(tr("Call"), this);
    accept_->setObjectName(QStringLiteral("callAccept"));
    accept_->setDefault(true);
    end_ = new QPushButton(tr("Close"), this);
    end_->setIcon(callIcon(3));
    end_->setIconSize(QSize(22, 22));
    end_->setObjectName(QStringLiteral("callEnd"));
    actions->addWidget(accept_);
    actions->addWidget(end_);
    layout->addLayout(actions);
    connect(accept_, &QPushButton::clicked, this, &CallView::accepted);
    connect(end_, &QPushButton::clicked, this, &CallView::ended);
}

void CallView::setPeer(const QString &peer, bool editable)
{
    peer_->setText(peer);
    peer_->setReadOnly(!editable);
}
QString CallView::peer() const { return peer_->text().trimmed(); }
void    CallView::setStatus(const QString &text) { status_->setText(text); }
void    CallView::setActive(bool active)
{
    active_ = active;
    accept_->setVisible(!active);
    quality_->setVisible(!active);
    end_->setText(active ? tr("End call") : tr("Close"));
    share_->setEnabled(active && shareSupported_ && ScreenShareCapture::available());
    if (active)
        placeholder_->setText(tr("Connected · waiting for video"));
}
void CallView::setConnecting(bool connecting)
{
    accept_->setEnabled(!connecting);
    peer_->setReadOnly(connecting);
    quality_->setEnabled(!connecting);
    camera_->setEnabled(!connecting && videoSupported_);
    end_->setText(connecting ? tr("Cancel") : tr("Close"));
}
void CallView::setIncoming(bool incoming) { accept_->setText(incoming ? tr("Accept call") : tr("Call")); }
void CallView::setVideoSupported(bool supported)
{
    videoSupported_ = supported;
    camera_->setEnabled(supported);
    if (!supported)
        camera_->setChecked(false);
}
void CallView::setSharingSupported(bool supported)
{
    shareSupported_ = supported;
    share_->setEnabled(active_ && supported && ScreenShareCapture::available());
}
void CallView::setCameraEnabled(bool enabled)
{
    const QSignalBlocker blocked(camera_);
    camera_->setChecked(enabled);
    camera_->setText(enabled ? tr("Camera on") : tr("Camera off"));
}
void CallView::setFinished()
{
    setActive(false);
    accept_->hide();
    quality_->hide();
    microphone_->setEnabled(false);
    camera_->setEnabled(false);
    share_->setEnabled(false);
    video_->hide();
    presentation_->hide();
    placeholder_->setText(tr("Call ended"));
    placeholder_->show();
    peer_->setReadOnly(true);
}
void CallView::setSharing(bool sharing, const QString &source, bool connecting)
{
    sharing_ = sharing;
    share_->setChecked(sharing);
    share_->setText(sharing ? (connecting ? tr("Connecting screen…") : tr("Sharing screen")) : tr("Share screen"));
    sharingLabel_->setText((connecting ? tr("Connecting screen sharing: %1") : tr("You are sharing: %1")).arg(source));
    sharingBar_->setVisible(sharing);
}
void CallView::setShareSelecting(bool selecting)
{
    selecting_ = selecting;
    share_->setEnabled(active_ && shareSupported_ && ScreenShareCapture::available());
    share_->setText(selecting ? tr("Cancel selection") : (sharing_ ? tr("Sharing screen") : tr("Share screen")));
}
void CallView::setDuration(qint64 seconds)
{
    duration_->setText(QStringLiteral("%1:%2:%3")
                           .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
                           .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
                           .arg(seconds % 60, 2, 10, QLatin1Char('0')));
}
bool                   CallView::cameraEnabled() const { return camera_->isChecked(); }
bool                   CallView::microphoneEnabled() const { return microphone_->isChecked(); }
int                    CallView::bitrate() const { return quality_->currentData().toInt(); }
PsiMedia::VideoWidget *CallView::remoteVideo() const { return video_; }
PsiMedia::VideoWidget *CallView::remotePresentation() const { return presentation_; }

void CallView::hideRemoteVideo(bool presentation)
{
    (presentation ? presentation_ : video_)->hide();
    if (video_->isHidden() && presentation_->isHidden())
        placeholder_->show();
}
