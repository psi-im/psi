/* SPDX-License-Identifier: GPL-2.0-or-later */
// Test-only Qt generic plugin. Drives the real executable through its actual
// scrollbar and inspects rendered Qt widgets, without linking Psi internals.
#include <QAbstractScrollArea>
#include <QApplication>
#include <QFrame>
#include <QGenericPlugin>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QtTest>

class Probe final : public QObject {
public:
    Probe()
    {
        auto *timer = new QTimer(this);
        timer->setInterval(25);
        connect(timer, &QTimer::timeout, this, [this] { tick(); });
        timer->start();
    }

private:
    int  phase_ = 0, settled_ = 0, originalMaximum_ = 0;
    void tick()
    {
        if (phase_ == 2)
            return;
        QAbstractScrollArea *area = nullptr;
        for (auto *widget : QApplication::allWidgets())
            if (widget->inherits("MamChatWidget") && widget->isVisible()) {
                area = qobject_cast<QAbstractScrollArea *>(widget);
                break;
            }
        if (!area)
            return;
        int        measured = 0, visiblePixels = 0, materialized = 0, middle = -1;
        bool       fallback = false;
        const auto viewport = area->viewport();
        const auto rows     = viewport->findChildren<QFrame *>("mamMessageRow");
        const int  total    = qEnvironmentVariableIntValue("MAM_RUNTIME_MESSAGE_COUNT");
        for (const auto *row : rows) {
            const auto *body = row->findChild<QTextEdit *>("mamBody");
            if (!body)
                continue;
            const QString text = body->toPlainText();
            fallback |= text.contains("cannot position messages by index");
            if (row->property("_psiMamInformational").toBool())
                continue;
            ++materialized;
            measured += row->height();
            const auto rect = QRect(row->mapTo(viewport, QPoint()), row->size()).intersected(viewport->rect());
            visiblePixels += rect.height();
            const auto match = QRegularExpression("Runtime message (\\d+)").match(text);
            if (!rect.isEmpty() && match.hasMatch()) {
                const int number = match.captured(1).toInt();
                if (number >= total / 4 && number <= total * 3 / 4)
                    middle = number;
            }
        }
        auto *bar = area->verticalScrollBar();
        if (phase_ == 0) {
            // Real materialized heights and clipped viewport coverage, not
            // estimated logical heights or a production fill-helper call.
            const bool filled = measured >= viewport->height() * 3 / 2 && visiblePixels >= viewport->height() - 2
                && bar->value() == bar->maximum();
            settled_ = filled ? settled_ + 1 : 0;
            if (settled_ < 3)
                return;
            qInfo().noquote() << "MAM_PROBE_INITIAL_FILLED measured=" << measured << "viewport=" << viewport->height()
                              << "visiblePixels=" << visiblePixels << "widgets=" << materialized;
            const auto mode = qEnvironmentVariable("MAM_RUNTIME_PROBE_MODE");
            if (mode == "initial" || mode.isEmpty()) {
                phase_ = 2;
                return;
            }
            phase_           = 1;
            originalMaximum_ = bar->maximum();
            bar->setSliderDown(true);
            bar->setSliderPosition(bar->maximum() / 2);
            bar->setSliderDown(false);
            qInfo() << "MAM_PROBE_SEEK_STARTED maximum=" << originalMaximum_;
        } else if (phase_ == 1) {
            if (qEnvironmentVariable("MAM_RUNTIME_PROBE_MODE") == "fallback") {
                if (fallback && materialized > 0 && bar->maximum() < originalMaximum_ / 2) {
                    qInfo() << "MAM_PROBE_SEEK_FALLBACK widgets=" << materialized;
                    phase_ = 2;
                }
            } else if (middle >= 0 && visiblePixels >= viewport->height() - 2 && materialized <= 160) {
                qInfo() << "MAM_PROBE_SEEK_FILLED messageNumber=" << middle << "widgets=" << materialized;
                phase_ = 2;
            }
        }
    }
};
class ProbePlugin final : public QGenericPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QGenericPluginFactoryInterface" FILE "probe.json")
public:
    QObject *create(const QString &name, const QString &) override
    {
        return name == "mam-history-probe" ? new Probe : nullptr;
    }
};
#include "probe.moc"
