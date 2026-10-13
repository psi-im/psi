// SPDX-License-Identifier: GPL-2.0-or-later
#include "callview.h"
#include "callstage.h"
#include "../psimedia/psimedia.h"
#include "screensharecapture.h"
#include <QComboBox>
#include <QEvent>
#include <QFontMetrics>
#include <QPalette>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {
QIcon callIcon(int kind, const QColor &color, bool inactive = false)
{
    QPixmap image(48, 48);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
    } else if (kind == 4) {
        painter.drawRoundedRect(QRectF(2, 3, 20, 14), 1.5, 1.5);
        painter.drawLine(QPointF(9, 17), QPointF(6, 22));
        painter.drawLine(QPointF(15, 17), QPointF(18, 22));
        QPainterPath stroke;
        stroke.moveTo(6, 13);
        stroke.cubicTo(9, 6, 12, 14, 18, 7);
        painter.drawPath(stroke);
    } else {
        QPainterPath receiver;
        receiver.moveTo(3, 14);
        receiver.cubicTo(6, 6, 18, 6, 21, 14);
        painter.drawPath(receiver);
        painter.drawLine(QPointF(3, 14), QPointF(7, 16));
        painter.drawLine(QPointF(21, 14), QPointF(17, 16));
    }
    if (inactive && (kind == 0 || kind == 1))
        painter.drawLine(QPointF(3, 3), QPointF(21, 21));
    return QIcon(image);
}

QPixmap statusIcon(const QColor &color, bool secure = false, bool warning = false)
{
    QPixmap image(20, 20);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (secure || warning) {
        if (warning) {
            painter.drawPolygon(QPolygonF() << QPointF(10, 2) << QPointF(18, 17) << QPointF(2, 17));
            painter.drawLine(QPointF(10, 7), QPointF(10, 12));
            painter.drawPoint(QPointF(10, 15));
        } else {
            painter.drawArc(QRectF(5, 2, 10, 11), 0, 180 * 16);
            painter.drawRoundedRect(QRectF(4, 9, 12, 9), 2, 2);
        }
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QRectF(6, 6, 8, 8));
    }
    return image;
}
}

CallView::CallView(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("callView"));
    // Style only the call controls; never impose QLabel/QPushButton colors
    // on the embedded whiteboard or other child widgets.
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);
    auto header = new QHBoxLayout;
    header->setContentsMargins(2, 0, 2, 0);
    header->setSpacing(10);

    status_ = new QLabel(tr("Ready to call"), this);
    status_->setObjectName(QStringLiteral("callStatus"));
    status_->setTextFormat(Qt::PlainText);
    status_->setToolTip(tr("Ready to call"));
    status_->setMaximumWidth(360);
    header->addWidget(status_);

    security_ = new QLabel(this);
    security_->setObjectName(QStringLiteral("callSecurity"));
    security_->setTextFormat(Qt::PlainText);
    security_->hide();
    header->addWidget(security_);

    duration_ = new QLabel(this);
    duration_->setObjectName(QStringLiteral("callDuration"));
    duration_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    duration_->setVisible(false);
    header->addStretch(1);
    header->addWidget(duration_);
    layout->addLayout(header);

    stage_ = new CallStage(this);
    stage_->setMinimumSize(420, 260);
    stage_->setPlaceholderText(tr("Audio call\nTurn on your camera or share your screen"));
    video_        = new PsiMedia::VideoWidget;
    presentation_ = new PsiMedia::VideoWidget;
    stage_->addTile(presentation_, tr("Shared screen"), CallStage::ScreenPriority);
    stage_->addTile(video_, tr("Camera"), CallStage::CameraPriority);
    for (auto video : { video_, presentation_ }) {
        connect(video, &PsiMedia::VideoWidget::videoSizeChanged, this, [this, video] {
            if (active_)
                stage_->setTileVisible(video, true);
        });
    }
    layout->addWidget(stage_, 1);

    auto controlBar = new QFrame(this);
    controlBar->setObjectName(QStringLiteral("callControls"));
    auto controls = new QHBoxLayout(controlBar);
    controls->setContentsMargins(8, 6, 8, 6);
    controls->setSpacing(6);

    microphone_ = new QPushButton(this);
    microphone_->setObjectName(QStringLiteral("callMicrophone"));
    microphone_->setCheckable(true);
    microphone_->setChecked(true);
    connect(microphone_, &QPushButton::toggled, this, [this](bool enabled) {
        updateControlPresentation();
        emit microphoneChanged(enabled);
    });

    camera_ = new QPushButton(this);
    camera_->setObjectName(QStringLiteral("callCamera"));
    camera_->setCheckable(true);
    connect(camera_, &QPushButton::toggled, this, [this](bool enabled) {
        updateControlPresentation();
        emit cameraChanged(enabled);
    });

    share_ = new QPushButton(this);
    share_->setObjectName(QStringLiteral("callShare"));
    share_->setCheckable(true);
    share_->setEnabled(false);
    connect(share_, &QPushButton::clicked, this, [this] {
        // Clicking opens a picker; only the backend confirms active sharing.
        share_->setChecked(sharing_);
        emit shareRequested();
    });

    whiteboard_ = new QPushButton(this);
    whiteboard_->setObjectName(QStringLiteral("callWhiteboard"));
    whiteboard_->setCheckable(true);
    whiteboard_->setEnabled(false);
    connect(whiteboard_, &QPushButton::toggled, this, [this](bool enabled) {
        updateControlPresentation();
        emit whiteboardChanged(enabled);
    });
    for (auto button : { microphone_, camera_, share_, whiteboard_ }) {
        button->setIconSize(QSize(21, 21));
        button->setFixedSize(44, 42);
        button->setCursor(Qt::PointingHandCursor);
    }

    sharingHint_ = new QLabel(controlBar);
    sharingHint_->setObjectName(QStringLiteral("callSharingHint"));
    sharingHint_->setTextFormat(Qt::PlainText);
    sharingHint_->setMaximumWidth(220);
    sharingHint_->hide();

    quality_ = new QComboBox(this);
    quality_->setObjectName(QStringLiteral("callQuality"));
    quality_->addItem(tr("HD · 2 Mbps"), 2000);
    quality_->addItem(tr("Balanced · 1 Mbps"), 1000);
    quality_->addItem(tr("Low bandwidth · 400 kbps"), 400);
    quality_->setCurrentIndex(1);
    quality_->setAccessibleName(tr("Video quality"));

    accept_ = new QPushButton(tr("Call"), this);
    accept_->setObjectName(QStringLiteral("callAccept"));
    accept_->setDefault(true);

    end_ = new QPushButton(tr("Close"), this);
    end_->setObjectName(QStringLiteral("callEnd"));
    end_->setIconSize(QSize(20, 20));

    controls->addWidget(microphone_);
    controls->addWidget(camera_);
    controls->addWidget(share_);
    controls->addWidget(whiteboard_);
    controls->addWidget(sharingHint_);
    controls->addStretch(1);
    controls->addWidget(quality_);
    controls->addWidget(accept_);
    controls->addWidget(end_);
    layout->addWidget(controlBar);

    updateAppearance();
    updateControlPresentation();
    connect(accept_, &QPushButton::clicked, this, &CallView::accepted);
    connect(end_, &QPushButton::clicked, this, &CallView::ended);
}

void CallView::setPeer(const QString &peer)
{
    peer_ = peer;
    emit headingChanged();
}
QString CallView::peer() const { return peer_.trimmed(); }

void CallView::setSecurity(const QString &text, bool endToEnd, bool partiallyEncrypted)
{
    security_->setToolTip(text);
    security_->setAccessibleName(text);
    security_->setProperty("endToEnd", endToEnd);
    security_->setProperty("partialSecurity", partiallyEncrypted);
    // A server-backed whiteboard is not covered by the media encryption.
    const bool verified = endToEnd && !partiallyEncrypted;
    const QColor ink = verified ? QColor(QStringLiteral("#16976c")) : QColor(QStringLiteral("#c48622"));
    security_->setPixmap(statusIcon(ink, verified, !verified));
    security_->setVisible(!text.isEmpty());
}

void CallView::setStatus(const QString &text)
{
    status_->setToolTip(text);
    status_->setAccessibleName(text);
    if (text == tr("Connected")) {
        status_->setText(QString());
        status_->setPixmap(statusIcon(QColor(QStringLiteral("#16976c"))));
    } else {
        status_->setPixmap(QPixmap());
        status_->setText(status_->fontMetrics().elidedText(text, Qt::ElideRight, 350));
    }
}

void CallView::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)
        updateAppearance();
}

void CallView::updateAppearance()
{
    if (updatingAppearance_)
        return;
    updatingAppearance_ = true;
    const auto colors = palette();
    const QColor surface = colors.color(QPalette::Window);
    const QColor canvas = colors.color(QPalette::Base);
    const QColor text = colors.color(QPalette::WindowText);
    const QColor muted = colors.color(QPalette::Disabled, QPalette::WindowText);
    const QColor button = colors.color(QPalette::Button);
    const QColor buttonText = colors.color(QPalette::ButtonText);
    const QColor border = colors.color(QPalette::Mid);
    const QColor accent = colors.color(QPalette::Highlight);
    const QColor accentText = colors.color(QPalette::HighlightedText);
    // Scoped selectors leave the whiteboard's native canvas and toolbar alone.
    setStyleSheet(QStringLiteral(
        "QWidget#callView { background-color: %1; color: %2; }"
        "QLabel#callStatus, QLabel#callDuration, QLabel#callSharingHint { color: %4; }"
        "QFrame#callStage { background: %3; border: 1px solid %6; border-radius: 10px; }"
        "QFrame#callTile { background: %3; border: none; border-radius: 6px; }"
        "QFrame#callControls { background: %5; border: 1px solid %6; border-radius: 10px; }"
        "QPushButton#callMicrophone, QPushButton#callCamera, QPushButton#callShare,"
        " QPushButton#callWhiteboard { color: %7; background: %5; border: 1px solid %6;"
        " border-radius: 8px; padding: 6px; }"
        "QPushButton#callMicrophone:hover, QPushButton#callCamera:hover,"
        " QPushButton#callShare:hover, QPushButton#callWhiteboard:hover { border-color: %8; }"
        "QPushButton#callMicrophone:checked, QPushButton#callCamera:checked,"
        " QPushButton#callShare:checked, QPushButton#callWhiteboard:checked"
        " { color: %9; background: %8; border-color: %8; }"
        "QPushButton#callMicrophone:focus, QPushButton#callCamera:focus,"
        " QPushButton#callShare:focus, QPushButton#callWhiteboard:focus"
        " { border: 2px solid %8; }"
        "QPushButton#callMicrophone:disabled, QPushButton#callCamera:disabled,"
        " QPushButton#callShare:disabled, QPushButton#callWhiteboard:disabled"
        " { color: %4; background: %5; border-color: %6; }"
        "QPushButton#callEnd { color: white; background: #bf3650; border: none;"
        " border-radius: 8px; padding: 10px 13px; font-weight: 600; }"
        "QPushButton#callEnd:hover { background: #a92f46; }"
        "QPushButton#callEnd:focus { border: 2px solid %8; }"
        "QPushButton#callAccept { color: %9; background: %8; border: none;"
        " border-radius: 8px; padding: 10px 13px; font-weight: 600; }"
        "QComboBox#callQuality { color: %7; background: %5; border: 1px solid %6;"
        " border-radius: 7px; padding: 7px; }")
        .arg(surface.name(), text.name(), canvas.name(), muted.name(), button.name(),
             border.name(), buttonText.name(), accent.name(), accentText.name()));
    updateControlPresentation();
    if (!security_->toolTip().isEmpty())
        setSecurity(security_->toolTip(), security_->property("endToEnd").toBool(),
                    security_->property("partialSecurity").toBool());
    updatingAppearance_ = false;
}

void CallView::updateControlPresentation()
{
    const auto colors = palette();
    const QColor regular = colors.color(QPalette::ButtonText);
    const QColor selected = colors.color(QPalette::HighlightedText);
    microphone_->setToolTip(microphone_->isChecked() ? tr("Mute microphone") : tr("Unmute microphone"));
    camera_->setToolTip(camera_->isChecked() ? tr("Turn camera off") : tr("Turn camera on"));
    share_->setToolTip(selecting_ ? tr("Cancel screen selection")
                     : sharing_ ? tr("Stop sharing: %1").arg(shareSource_)
                                : tr("Share a display or window"));
    whiteboard_->setToolTip(whiteboard_->isChecked() ? tr("Hide whiteboard") : tr("Show whiteboard"));
    microphone_->setAccessibleName(microphone_->toolTip());
    camera_->setAccessibleName(camera_->toolTip());
    share_->setAccessibleName(share_->toolTip());
    whiteboard_->setAccessibleName(whiteboard_->toolTip());
    microphone_->setIcon(callIcon(0, microphone_->isChecked() ? selected : regular, !microphone_->isChecked()));
    camera_->setIcon(callIcon(1, camera_->isChecked() ? selected : regular, !camera_->isChecked()));
    share_->setIcon(callIcon(2, sharing_ ? selected : regular));
    whiteboard_->setIcon(callIcon(4, whiteboard_->isChecked() ? selected : regular));
    end_->setIcon(callIcon(3, QColor(Qt::white)));
    sharingHint_->setVisible(sharing_ || selecting_);
    const QString hint = selecting_ ? tr("Selecting screen…")
                         : shareConnecting_ ? tr("Connecting screen…")
                                            : tr("Sharing: %1").arg(shareSource_);
    sharingHint_->setText(sharingHint_->fontMetrics().elidedText(hint, Qt::ElideMiddle, 205));
    sharingHint_->setToolTip(hint);
    sharingHint_->setAccessibleName(hint);
}

void    CallView::setActive(bool active)
{
    active_ = active;
    accept_->setVisible(!active);
    quality_->setVisible(!active);
    end_->setText(active ? (whiteboardOnly_ ? tr("End") : tr("End call")) : tr("Close"));
    updateMediaButtons();
    duration_->setVisible(active);
    if (active)
        stage_->setPlaceholderText(whiteboardOnly_ ? tr("Connected") : tr("Connected · waiting for video"));
}
void CallView::setConnecting(bool connecting)
{
    accept_->setEnabled(!connecting);
    quality_->setEnabled(!connecting && !whiteboardOnly_);
    connecting_ = connecting;
    updateMediaButtons();
    end_->setText(connecting ? tr("Cancel") : active_ ? (whiteboardOnly_ ? tr("End") : tr("End call"))
                                                        : tr("Close"));
}
void CallView::setIncoming(bool incoming)
{
    if (whiteboardOnly_)
        accept_->setText(incoming ? tr("Join whiteboard") : tr("Invite"));
    else
        accept_->setText(incoming ? tr("Accept call") : tr("Call"));
}
void CallView::setVideoSupported(bool supported)
{
    videoSupported_ = supported;
    updateMediaButtons();
    if (!supported)
        camera_->setChecked(false);
}
void CallView::setSharingSupported(bool supported)
{
    shareSupported_ = supported;
    updateMediaButtons();
}
void CallView::setCameraEnabled(bool enabled)
{
    const QSignalBlocker blocked(camera_);
    camera_->setChecked(enabled);
    updateControlPresentation();
}
void CallView::setFinished()
{
    finished_ = true;
    setActive(false);
    accept_->hide();
    quality_->hide();
    updateMediaButtons();
    whiteboard_->setEnabled(false);
    stage_->hideAllTiles();
    stage_->setPlaceholderText(whiteboardOnly_ ? tr("Session ended") : tr("Call ended"));
}
void CallView::setSharing(bool sharing, const QString &source, bool connecting)
{
    sharing_ = sharing;
    shareConnecting_ = sharing && connecting;
    shareSource_ = sharing ? source : QString();
    share_->setChecked(sharing);
    updateControlPresentation();
}
void CallView::setShareSelecting(bool selecting)
{
    selecting_ = selecting;
    updateMediaButtons();
    updateControlPresentation();
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
CallStage             *CallView::stage() const { return stage_; }

void CallView::setWhiteboardOnly(bool whiteboardOnly)
{
    whiteboardOnly_ = whiteboardOnly;
    emit headingChanged();
    if (whiteboardOnly) {
        // Screen sharing needs running media; it is offered again once audio or video is connected.
        shareSupported_ = false;
        const QSignalBlocker blocked(microphone_);
        microphone_->setChecked(false);
        updateControlPresentation();
    }
    quality_->setEnabled(!whiteboardOnly);
    updateMediaButtons();
    stage_->setPlaceholderText(whiteboardOnly ? tr("Whiteboard")
                                              : tr("Audio call\nTurn on your camera or share your screen"));
}
bool CallView::whiteboardOnly() const { return whiteboardOnly_; }
void CallView::setMediaAvailable(bool available)
{
    mediaAvailable_ = available;
    updateMediaButtons();
}
void CallView::setMicrophoneChecked(bool enabled)
{
    const QSignalBlocker blocked(microphone_);
    microphone_->setChecked(enabled);
    updateControlPresentation();
}
void CallView::updateMediaButtons()
{
    // A session that started as a whiteboard gets media once it is connected, if both sides can use it.
    const bool media = !finished_ && mediaAvailable_ && (!whiteboardOnly_ || active_);
    microphone_->setEnabled(media);
    camera_->setEnabled(media && videoSupported_ && !connecting_);
    share_->setEnabled(media && active_ && shareSupported_ && ScreenShareCapture::available());
    const QString unavailable = whiteboardOnly_ && !mediaAvailable_ ? tr("Calls are not available with this contact")
                                                                    : QString();
    updateControlPresentation();
    if (!unavailable.isEmpty()) {
        microphone_->setToolTip(unavailable);
        camera_->setToolTip(unavailable);
        microphone_->setAccessibleName(unavailable);
        camera_->setAccessibleName(unavailable);
    }
}
void CallView::setWhiteboardSupported(bool supported)
{
    whiteboardSupported_ = supported;
    whiteboard_->setEnabled(supported && !finished_);
}
void CallView::setWhiteboardEnabled(bool enabled)
{
    const QSignalBlocker blocked(whiteboard_);
    whiteboard_->setChecked(enabled);
    updateControlPresentation();
}
bool CallView::whiteboardEnabled() const { return whiteboard_->isChecked(); }

void CallView::hideRemoteVideo(bool presentation)
{
    stage_->setTileVisible(presentation ? presentation_ : video_, false);
}
