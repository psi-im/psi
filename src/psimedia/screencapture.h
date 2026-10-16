// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <memory>

namespace PsiMedia {
// Optional capture API, independent of the plugin's media framework. All methods
// use the GUI thread. Only startSelection() may ask for desktop permission.
class ScreenCapture : public QObject {
    Q_OBJECT
public:
    struct Source {
        enum Kind { Display, Window } kind = Display;
        QString    id;
        QString    label;
        QByteArray thumbnail; // optional PNG preview
    };
    explicit ScreenCapture(QObject *parent = nullptr);
    ~ScreenCapture() override;
    static bool                 available();
    void                        startSelection();
    QList<Source>               sources() const;
    void                        choose(const QString &id);
    void                        stop();
    std::shared_ptr<const void> sourceLease() const;
signals:
    void sourcesAvailable();
    void ready(const QString &deviceId, const QString &label);
    void stopped();
    void failed(const QString &message);

private:
    Q_DISABLE_COPY(ScreenCapture)
    class Private;
    Private *d;
};

}
