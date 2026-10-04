// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SCREENSHARECAPTURE_H
#define SCREENSHARECAPTURE_H

#include <QObject>
#include <QPointer>
#include <QVariantMap>
#include <memory>
#ifdef PSI_SCREENSHARE_PORTAL
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#endif

class QWidget;
class QDialog;

// Owns the user's capture permission and (on Wayland) the PipeWire descriptor.
// Consumers must quiesce the video source synchronously when stopped() is emitted.
class ScreenShareCapture : public QObject {
    Q_OBJECT
public:
    explicit ScreenShareCapture(QObject *parent = nullptr);
    ~ScreenShareCapture() override;
    static bool                 available();
    bool                        selecting() const;
    std::shared_ptr<const void> sourceLease() const { return sourceLease_; }
    void                        select(QWidget *parent);
    void                        stop();

signals:
    void ready(const QString &source, const QString &label);
    void stopped();
    void selectingChanged(bool selecting);
    void failed(const QString &message);

private:
    void                        selectX11(QWidget *parent);
    void                        fail(const QString &message);
    QPointer<QDialog>           picker_;
    bool                        active_    = false;
    bool                        selecting_ = false;
    std::shared_ptr<const void> sourceLease_;
    quint64                     generation_ = 0;
#ifdef PSI_SCREENSHARE_PORTAL
    enum class Step { Idle, Create, Select, Start, Open };
    Step                    step_ = Step::Idle;
    QString                 requestPath_;
    QString                 sessionPath_;
    QString                 parentWindow_;
    QDBusUnixFileDescriptor pipeWireFd_;
    uint                    sourceTypes_ = 3;
    bool                    embedCursor_ = false;
    void                    requestPortal(const QString &method, const QVariantList &arguments, QVariantMap options);
    void                    openPipeWire(uint node, const QVariantMap &properties);
private slots:
    void portalResponse(uint response, const QVariantMap &results, const QDBusMessage &message);
    void portalClosed(const QVariantMap &details);
#endif
};

#endif
