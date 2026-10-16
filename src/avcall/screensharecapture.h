// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SCREENSHARECAPTURE_H
#define SCREENSHARECAPTURE_H

#include "screencapture.h"
#include <QPointer>

class QDialog;
class QWidget;

// Owns the application's selection UI; the media provider owns all platform
// capture details and permissions. Consumers quiesce capture on stopped().
class ScreenShareCapture : public QObject {
    Q_OBJECT
public:
    explicit ScreenShareCapture(QObject *parent = nullptr);
    ~ScreenShareCapture() override;
    static bool                 available();
    bool                        selecting() const;
    QString                     source() const { return source_; }
    std::shared_ptr<const void> sourceLease() const { return backend_.sourceLease(); }
    void                        select(QWidget *parent);
    void                        stop();
signals:
    void ready(const QString &deviceId, const QString &label);
    void stopped();
    void selectingChanged(bool selecting);
    void failed(const QString &message);

private:
    void                    showSources();
    void                    finish();
    PsiMedia::ScreenCapture backend_;
    QPointer<QDialog>       picker_;
    QPointer<QWidget>       parent_;
    QString                 source_;
    bool                    active_     = false;
    bool                    selecting_  = false;
    quint64                 generation_ = 0;
};
#endif
