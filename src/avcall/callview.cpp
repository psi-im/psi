// SPDX-License-Identifier: GPL-2.0-or-later
#include "callview.h"
#include "../psimedia/psimedia.h"
#include "callstage.h"
#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPolygonF>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QToolButton>
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

    microphoneDevices_ = new QToolButton(this);
    cameraDevices_     = new QToolButton(this);
    microphoneDevices_->setObjectName(QStringLiteral("callMicrophoneDevices"));
    cameraDevices_->setObjectName(QStringLiteral("callCameraDevices"));
    microphoneMenu_ = new QMenu(microphoneDevices_);
    cameraMenu_     = new QMenu(cameraDevices_);
    microphoneDevices_->setMenu(microphoneMenu_);
    cameraDevices_->setMenu(cameraMenu_);
    for (auto *button : { microphoneDevices_, cameraDevices_ }) {
        button->setArrowType(Qt::DownArrow);
        button->setPopupMode(QToolButton::InstantPopup);
        button->setFixedSize(20, 42);
        button->setCursor(Qt::PointingHandCursor);
    }

    share_ = new QPushButton(this);
    share_->setObjectName(QStringLiteral("callShare"));
    share_->setCheckable(true);
    share_->setEnabled(false);
    connect(share_, &QPushButton::clicked, this, [this] {
        // Clicking opens/cancels selection; the controller owns its state.
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

    security_ = new QLabel(controlBar);
    security_->setObjectName(QStringLiteral("callSecurity"));
    security_->setTextFormat(Qt::PlainText);
    security_->setFixedSize(28, 32);
    security_->setAlignment(Qt::AlignCenter);
    security_->hide();

    microphoneDevices_->setToolTip(tr("Choose microphone"));
    microphoneDevices_->setAccessibleName(microphoneDevices_->toolTip());
    cameraDevices_->setToolTip(tr("Choose camera"));
    cameraDevices_->setAccessibleName(cameraDevices_->toolTip());

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
    end_->setMinimumWidth(120);
    end_->setToolTip(tr("Close call window"));
    end_->setAccessibleName(end_->toolTip());

    controls->addWidget(microphone_);
    controls->addWidget(microphoneDevices_);
    controls->addWidget(camera_);
    controls->addWidget(cameraDevices_);
    controls->addWidget(share_);
    controls->addWidget(whiteboard_);
    controls->addWidget(sharingHint_);
    controls->addStretch(1);
    controls->addWidget(quality_);
    controls->addWidget(accept_);
    controls->addWidget(security_);
    controls->addWidget(end_);
    layout->addWidget(controlBar);

    updateAppearance();
    updateMediaButtons();
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
    setStyleSheet(QStringLiteral("QWidget#callView { background-color: %1; color: %2; }"
                                 "QLabel#callSharingHint { color: %4; }"
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
                                 "QToolButton#callMicrophoneDevices, QToolButton#callCameraDevices"
                                 " { color: %7; background: %5; border: 1px solid %6; border-radius: 6px; }"
                                 "QToolButton#callMicrophoneDevices:hover, QToolButton#callCameraDevices:hover"
                                 " { border-color: %8; }"
                                 "QToolButton#callMicrophoneDevices::menu-indicator,"
                                 " QToolButton#callCameraDevices::menu-indicator { image: none; }"
                                 "QPushButton#callEnd { color: white; background: #bf3650; border: none;"
                                 " border-radius: 8px; padding: 10px 13px; font-weight: 600; }"
                                 "QPushButton#callEnd:hover { background: #a92f46; }"
                                 "QPushButton#callEnd:focus { border: 2px solid %8; }"
                                 "QPushButton#callAccept { color: %9; background: %8; border: none;"
                                 " border-radius: 8px; padding: 10px 13px; font-weight: 600; }"
                                 "QComboBox#callQuality { color: %7; background: %5; border: 1px solid %6;"
                                 " border-radius: 7px; padding: 7px; }")
                      .arg(surface.name(), text.name(), canvas.name(), muted.name(), button.name(), border.name(),
                           buttonText.name(), accent.name(), accentText.name()));
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
    share_->setToolTip(selecting_           ? tr("Cancel screen selection")
                           : sharePrepared_ ? tr("Cancel selected screen: %1").arg(shareSource_)
                           : sharing_       ? tr("Stop sharing: %1").arg(shareSource_)
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
    // The elapsed clock doubles as the live hang-up button label. The red
    // handset and accessible action name still identify its destructive role.
    if (active_) {
        end_->setText(elapsedText_);
        end_->setToolTip(tr("End call (%1 elapsed)").arg(elapsedText_));
        end_->setAccessibleName(end_->toolTip());
    } else if (connecting_) {
        end_->setText(tr("Cancel"));
        end_->setToolTip(tr("Cancel call"));
        end_->setAccessibleName(end_->toolTip());
    } else {
        end_->setText(tr("Close"));
        end_->setToolTip(tr("Close call window"));
        end_->setAccessibleName(end_->toolTip());
    }
    sharingHint_->setVisible(sharing_ || selecting_);
    const QString hint = selecting_ ? tr("Selecting screen…")
        : sharePrepared_            ? tr("Screen selected: %1").arg(shareSource_)
        : shareConnecting_          ? tr("Connecting screen…")
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
    updateControlPresentation();
    updateMediaButtons();
    if (active)
        stage_->setPlaceholderText(whiteboardOnly_ ? tr("Connected") : tr("Connected · waiting for video"));
}
void CallView::setConnecting(bool connecting)
{
    accept_->setEnabled(!connecting);
    quality_->setEnabled(!connecting && !whiteboardOnly_);
    connecting_ = connecting;
    updateMediaButtons();
    updateControlPresentation();
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
    connecting_ = false;
    setSharing(false);
    setShareSelecting(false);
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
    sharePrepared_   = false;
    sharing_ = sharing;
    shareConnecting_ = sharing && connecting;
    shareSource_ = sharing ? source : QString();
    share_->setChecked(sharing);
    updateMediaButtons();
}
void CallView::setSharePrepared(const QString &source)
{
    sharing_         = true;
    sharePrepared_   = true;
    shareConnecting_ = false;
    shareSource_     = source;
    share_->setChecked(true);
    updateMediaButtons();
}
void CallView::setShareSelecting(bool selecting)
{
    selecting_ = selecting;
    updateMediaButtons();
    updateControlPresentation();
}
void CallView::setDuration(qint64 seconds)
{
    elapsedText_ = QStringLiteral("%1:%2:%3")
                       .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
                       .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
                       .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    if (active_)
        updateControlPresentation();
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
        // Capture can be selected now; transmitting still waits for the session.
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
    microphoneDevices_->setEnabled(!finished_ && mediaAvailable_ && microphonesAvailable_);
    cameraDevices_->setEnabled(!finished_ && mediaAvailable_ && videoSupported_ && camerasAvailable_);
    // Selecting a source is allowed before a call starts. An existing selection
    // must also remain cancellable if media capabilities disappear meanwhile.
    share_->setEnabled(!finished_ && (sharing_ || selecting_ || (mediaAvailable_ && shareSupported_)));
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

void CallView::setInputDevices(const QList<InputDevice> &microphones, const QString &microphoneId,
                               const QList<InputDevice> &cameras, const QString &cameraId)
{
    const auto populate
        = [this](QToolButton *button, QMenu *menu, QActionGroup *&group, const QList<InputDevice> &devices,
                 const QString &selected, const QString &title, auto notify) {
              delete group;
              menu->clear();
              group            = new QActionGroup(menu);
              QString fallback = devices.isEmpty() ? tr("No devices available") : devices.first().name;
              for (const auto &device : devices)
                  if (device.isDefault)
                      fallback = device.name;
              auto *automatic = menu->addAction(tr("System default (%1)").arg(fallback));
              automatic->setData(QString());
              automatic->setCheckable(true);
              automatic->setChecked(selected.isEmpty());
              group->addAction(automatic);
              QString current = selected.isEmpty() ? automatic->text() : QString();
              for (const auto &device : devices) {
                  auto *action = menu->addAction(device.name);
                  action->setData(device.id);
                  action->setCheckable(true);
                  action->setChecked(device.id == selected);
                  group->addAction(action);
                  if (device.id == selected)
                      current = device.name;
              }
              if (current.isEmpty()) {
                  current       = tr("Device unavailable; using %1").arg(fallback);
                  auto *missing = menu->addAction(current);
                  missing->setData(selected);
                  missing->setCheckable(true);
                  missing->setChecked(true);
                  missing->setEnabled(false);
                  group->addAction(missing);
              }
              button->setToolTip(title + QStringLiteral(": ") + current);
              button->setAccessibleName(button->toolTip());
              connect(group, &QActionGroup::triggered, this, [button, title, notify](QAction *action) {
                  button->setToolTip(title + QStringLiteral(": ") + action->text());
                  button->setAccessibleName(button->toolTip());
                  notify(action->data().toString());
              });
          };
    microphonesAvailable_ = !microphones.isEmpty();
    camerasAvailable_     = !cameras.isEmpty();
    populate(microphoneDevices_, microphoneMenu_, microphoneDeviceGroup_, microphones, microphoneId,
             tr("Choose microphone"), [this](const QString &id) { emit microphoneDeviceSelected(id); });
    populate(cameraDevices_, cameraMenu_, cameraDeviceGroup_, cameras, cameraId, tr("Choose camera"),
             [this](const QString &id) { emit cameraDeviceSelected(id); });
    updateMediaButtons();
}
