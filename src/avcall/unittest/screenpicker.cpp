// SPDX-License-Identifier: GPL-2.0-or-later
#include "psimedia.h"
#include "psimediaprovider.h"
#include "psimediascreencapture.h"
#include "screensharecapture.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTest>

namespace {
QList<std::function<void(const QString &)>> pending;
class CaptureContext : public QObject, public PsiMedia::ScreenCaptureContext {
    Q_OBJECT
public:
    QObject                              *qobject() override { return this; }
    bool                                  selecting = false, active = false;
    quint64                               generation = 0;
    std::shared_ptr<const void>           lease;
    QList<PsiMedia::PScreenCaptureSource> choices;
    void                                  startSelection() override
    {
        stop();
        selecting                      = true;
        const auto               epoch = generation;
        QPointer<CaptureContext> guard(this);
        pending.append([guard, epoch](const QString &error) {
            if (!guard || epoch != guard->generation || !guard->selecting)
                return;
            if (!error.isEmpty()) {
                guard->stop();
                if (guard)
                    emit guard->failed(error);
                return;
            }
            guard->choices = { { PsiMedia::PScreenCaptureSource::Display, "display-choice", "Monitor", {} },
                               { PsiMedia::PScreenCaptureSource::Window, "window-choice", "Editor", {} } };
            emit guard->sourcesAvailable();
        });
    }
    QList<PsiMedia::PScreenCaptureSource> sources() const override { return choices; }
    void                                  choose(const QString &id) override
    {
        if (!selecting)
            return;
        selecting = false;
        active    = true;
        lease     = std::make_shared<int>(1);
        emit ready(id == "window-choice" ? "opaque-window-device" : "opaque-display-device", "Selected source");
    }
    void stop() override
    {
        ++generation;
        const bool notify = selecting || active;
        selecting = active = false;
        if (notify)
            emit stopped();
        if (!active)
            lease.reset();
    }
    std::shared_ptr<const void> sourceLease() const override { return lease; }
signals:
    void sourcesAvailable();
    void ready(const QString &, const QString &);
    void stopped();
    void failed(const QString &);
};
class LegacyProvider : public QObject, public PsiMedia::Provider {
    Q_OBJECT
    Q_INTERFACES(PsiMedia::Provider)
public:
    QObject                        *qobject() override { return this; }
    bool                            isInitialized() const override { return true; }
    QString                         creditName() const override { return {}; }
    QString                         creditText() const override { return {}; }
    PsiMedia::FeaturesContext      *createFeatures() override { return nullptr; }
    PsiMedia::RtpSessionContext    *createRtpSession() override { return nullptr; }
    PsiMedia::AudioRecorderContext *createAudioRecorder() override { return nullptr; }
};
class CaptureProvider : public LegacyProvider, public PsiMedia::ScreenCaptureProvider {
    Q_OBJECT
    Q_INTERFACES(PsiMedia::ScreenCaptureProvider)
public:
    bool                            screenCaptureAvailable() const override { return true; }
    PsiMedia::ScreenCaptureContext *createScreenCapture() override { return new CaptureContext; }
};
}
class ScreenPickerTest : public QObject {
    Q_OBJECT
    QDialog *picker()
    {
        for (auto *widget : QApplication::topLevelWidgets())
            if (auto *dialog = qobject_cast<QDialog *>(widget))
                if (dialog->isVisible())
                    return dialog;
        return nullptr;
    }
    void            completeSelection() { pending.takeFirst()({}); }
    CaptureProvider provider_;
    LegacyProvider  legacy_;
private slots:
    void init() { PsiMedia::setProvider(&provider_); }
    void legacyProviderHasNoScreenCapture()
    {
        PsiMedia::setProvider(&legacy_);
        QVERIFY(!ScreenShareCapture::available());
        ScreenShareCapture capture;
        QSignalSpy         failed(&capture, &ScreenShareCapture::failed);
        capture.select(nullptr);
        QCOMPARE(failed.count(), 1);
        QVERIFY(!capture.selecting());
        QVERIFY(!picker());
    }
    void cleanup()
    {
        pending.clear();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void explicitSelection()
    {
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready), stopped(&capture, &ScreenShareCapture::stopped);
        capture.select(nullptr);
        QVERIFY(capture.selecting());
        QVERIFY(capture.source().isEmpty());
        QVERIFY(!capture.sourceLease());
        QCOMPARE(ready.count(), 0);
        completeSelection();
        auto *dialog = picker();
        QVERIFY(dialog);
        auto *buttons = dialog->findChild<QDialogButtonBox *>();
        QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
        auto *tabs = dialog->findChild<QTabWidget *>();
        tabs->setCurrentIndex(1);
        qobject_cast<QListWidget *>(tabs->currentWidget())->setCurrentRow(0);
        QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
        buttons->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first().first().toString(), QString("opaque-window-device"));
        QCOMPARE(capture.source(), QString("opaque-window-device"));
        QVERIFY(!capture.selecting());
        std::weak_ptr<const void> lease = capture.sourceLease();
        QVERIFY(!lease.expired());
        QCoreApplication::processEvents();
        QCOMPARE(capture.source(), QString("opaque-window-device"));
        QVERIFY(!lease.expired()); // Permission survives while waiting for call acceptance.
        capture.stop();
        QVERIFY(capture.source().isEmpty());
        QVERIFY(!capture.sourceLease());
        QVERIFY(lease.expired());
        QCOMPARE(stopped.count(), 1);
        capture.stop();
        QCOMPARE(stopped.count(), 1);
    }
    void cancelledEnumeration()
    {
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready), failed(&capture, &ScreenShareCapture::failed);
        capture.select(nullptr);
        capture.stop();
        completeSelection();
        QVERIFY(!picker());
        QCOMPARE(ready.count(), 0);
        QCOMPARE(failed.count(), 0);
    }
    void cancelledPicker()
    {
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready);
        capture.select(nullptr);
        completeSelection();
        QVERIFY(picker());
        picker()->reject();
        QVERIFY(!capture.selecting());
        QCOMPARE(ready.count(), 0);
    }
    void permissionDenied()
    {
        ScreenShareCapture capture;
        QSignalSpy         failed(&capture, &ScreenShareCapture::failed);
        capture.select(nullptr);
        pending.takeFirst()("Permission denied");
        QCOMPARE(failed.count(), 1);
        QVERIFY(!capture.selecting());
        QVERIFY(!picker());
    }
    void destroyedDuringEnumeration()
    {
        auto *capture = new ScreenShareCapture;
        capture->select(nullptr);
        delete capture;
        completeSelection();
        QVERIFY(!picker());
    }
    void selectingAgainFromStoppedKeepsNewSelection()
    {
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready);
        capture.select(nullptr);
        completeSelection();
        auto *dialog = picker();
        QVERIFY(dialog);
        auto *tabs = dialog->findChild<QTabWidget *>();
        qobject_cast<QListWidget *>(tabs->currentWidget())->setCurrentRow(0);
        dialog->accept();
        QCOMPARE(ready.count(), 1);
        connect(&capture, &ScreenShareCapture::stopped, &capture, [&] { capture.select(nullptr); });
        capture.stop();
        QVERIFY(capture.selecting());
        completeSelection();
        QVERIFY(picker());
        QVERIFY(capture.selecting());
        // Do not reselect from destruction.
        disconnect(&capture, &ScreenShareCapture::stopped, &capture, nullptr);
    }
    void stopDuringAcceptance()
    {
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready);
        connect(&capture, &ScreenShareCapture::selectingChanged, &capture, [&](bool selecting) {
            if (!selecting)
                capture.stop();
        });
        capture.select(nullptr);
        completeSelection();
        auto *dialog = picker();
        QVERIFY(dialog);
        auto *tabs = dialog->findChild<QTabWidget *>();
        qobject_cast<QListWidget *>(tabs->currentWidget())->setCurrentRow(0);
        dialog->accept();
        QCOMPARE(ready.count(), 0);
        QVERIFY(capture.source().isEmpty());
        QVERIFY(!capture.sourceLease());
    }
};
QTEST_MAIN(ScreenPickerTest)
#include "screenpicker.moc"
