// SPDX-License-Identifier: GPL-2.0-or-later
#include "callview.h"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>
class ScreenShareTest : public QObject {
    Q_OBJECT
private slots:
    void compactCallControlsAndStatus()
    {
        CallView view;
        auto     security = view.findChild<QLabel *>(QStringLiteral("callSecurity"));
        auto     hint     = view.findChild<QLabel *>(QStringLiteral("callSharingHint"));
        auto     share    = view.findChild<QPushButton *>(QStringLiteral("callShare"));
        auto     board    = view.findChild<QPushButton *>(QStringLiteral("callWhiteboard"));
        auto     end      = view.findChild<QPushButton *>(QStringLiteral("callEnd"));
        auto     dock     = view.findChild<QFrame *>(QStringLiteral("callControls"));
        QVERIFY(security && hint && share && board && end && dock);
        QVERIFY(!view.findChild<QLabel *>(QStringLiteral("callStatus")));
        QVERIFY(!view.findChild<QLabel *>(QStringLiteral("callDuration")));
        QCOMPARE(security->parentWidget(), static_cast<QWidget *>(dock));
        QVERIFY(!view.findChild<QWidget *>(QStringLiteral("callPeer")));
        QVERIFY(!view.findChild<QFrame *>(QStringLiteral("sharingBar")));
        QSignalSpy changed(&view, &CallView::headingChanged);
        view.setPeer(QStringLiteral("peer@example.test/device"));
        QCOMPARE(view.peer(), QStringLiteral("peer@example.test/device"));
        QCOMPARE(changed.count(), 1);
        QVERIFY(view.findChildren<QLineEdit *>().isEmpty());
        view.setSecurity(QStringLiteral("End-to-end encrypted, verified with OMEMO"), true);
        QVERIFY(!security->isHidden());
        QCOMPARE(security->accessibleName(), security->toolTip());
        view.setSecurity(QStringLiteral("Whiteboard travels through the server"), true, true);
        QCOMPARE(security->property("partialSecurity").toBool(), true);
        QCOMPARE(security->accessibleName(), security->toolTip());
        view.setConnecting(true);
        QCOMPARE(end->text(), QStringLiteral("Cancel"));
        view.setConnecting(false);
        view.setActive(true);
        QCOMPARE(end->text(), QStringLiteral("00:00:00"));
        view.setDuration(127);
        QCOMPARE(end->text(), QStringLiteral("00:02:07"));
        QVERIFY(end->toolTip().contains(QStringLiteral("End call")));
        QVERIFY(end->accessibleName().contains(QStringLiteral("00:02:07")));
        view.setDuration(3605);
        QCOMPARE(end->text(), QStringLiteral("01:00:05"));

        view.setSharing(true, QStringLiteral("Display 1 — 1920x1080"));
        QVERIFY(share->isChecked());
        QVERIFY(!hint->isHidden());
        QVERIFY(share->toolTip().contains(QStringLiteral("Display 1")));
        view.setSharing(false);
        QVERIFY(!share->isChecked());
        QVERIFY(hint->isHidden());
        view.setShareSelecting(true);
        QVERIFY(!hint->isHidden());
        QVERIFY(share->accessibleName().contains(QStringLiteral("Cancel")));
        view.setShareSelecting(false);
        QVERIFY(hint->isHidden());

        view.setWhiteboardSupported(true);
        view.setWhiteboardEnabled(true);
        QVERIFY(board->isChecked());
        view.setWhiteboardOnly(true);
        QVERIFY(changed.count() >= 2);
        view.setFinished();
        QCOMPARE(end->text(), QStringLiteral("Close"));
        QVERIFY(security->parentWidget() == dock);

        CallView pending;
        auto     pendingEnd = pending.findChild<QPushButton *>(QStringLiteral("callEnd"));
        QVERIFY(pendingEnd);
        pending.setConnecting(true);
        QCOMPARE(pendingEnd->text(), QStringLiteral("Cancel"));
        pending.setFinished();
        QCOMPARE(pendingEnd->text(), QStringLiteral("Close"));
    }

    void inputDeviceSelectionDoesNotEnableCapture()
    {
        CallView                           view;
        const QList<CallView::InputDevice> microphones { { "mic-a", "Desk microphone", true },
                                                         { "mic-b", "Headset microphone", false } };
        const QList<CallView::InputDevice> cameras { { "camera-a", "Built-in camera", true },
                                                     { "camera-b", "USB camera", false } };
        view.setMicrophoneChecked(false);
        view.setCameraEnabled(false);
        view.setInputDevices(microphones, "mic-a", cameras, "camera-a");
        auto *mic    = view.findChild<QToolButton *>(QStringLiteral("callMicrophoneDevices"));
        auto *camera = view.findChild<QToolButton *>(QStringLiteral("callCameraDevices"));
        QVERIFY(mic && camera && mic->isEnabled() && camera->isEnabled());
        QSignalSpy micDevice(&view, &CallView::microphoneDeviceSelected);
        QSignalSpy cameraDevice(&view, &CallView::cameraDeviceSelected);
        QSignalSpy micToggle(&view, &CallView::microphoneChanged);
        QSignalSpy cameraToggle(&view, &CallView::cameraChanged);
        const auto choose = [](QMenu *menu, const QString &id) {
            for (auto *action : menu->actions())
                if (action->data().toString() == id) {
                    action->trigger();
                    return true;
                }
            return false;
        };
        QVERIFY(choose(mic->menu(), "mic-b"));
        QVERIFY(choose(camera->menu(), "camera-b"));
        QCOMPARE(micDevice.count(), 1);
        QCOMPARE(micDevice.first().first().toString(), QString("mic-b"));
        QCOMPARE(cameraDevice.count(), 1);
        QCOMPARE(cameraDevice.first().first().toString(), QString("camera-b"));
        QCOMPARE(micToggle.count(), 0);
        QCOMPARE(cameraToggle.count(), 0);
        QVERIFY(!view.microphoneEnabled() && !view.cameraEnabled());
        view.setConnecting(true);
        QVERIFY(mic->isEnabled() && camera->isEnabled());
        QVERIFY(choose(camera->menu(), QString()));
        QCOMPARE(cameraDevice.last().first().toString(), QString());
        view.setFinished();
        QVERIFY(!mic->isEnabled() && !camera->isEnabled());
    }

    void deviceRefreshPreservesIdentityWithoutSelectionSignals()
    {
        CallView                           view;
        const QList<CallView::InputDevice> devices { { "one", "Camera", true }, { "two", "Camera", false } };
        QSignalSpy                         selected(&view, &CallView::cameraDeviceSelected);
        view.setInputDevices({}, {}, devices, "two");
        auto *camera = view.findChild<QToolButton *>(QStringLiteral("callCameraDevices"));
        QVERIFY(camera && camera->isEnabled());
        const auto checkedId = [](QMenu *menu) {
            for (auto *action : menu->actions())
                if (action->isChecked())
                    return action->data().toString();
            return QStringLiteral("no-selection");
        };
        QCOMPARE(checkedId(camera->menu()), QString("two"));
        view.setInputDevices({}, {}, { devices.first() }, "two");
        QCOMPARE(checkedId(camera->menu()), QString("two"));
        QVERIFY(!camera->menu()->actions().last()->isEnabled());
        QVERIFY(camera->toolTip().contains(QStringLiteral("unavailable")));
        view.setInputDevices({}, {}, devices, "two");
        QCOMPARE(checkedId(camera->menu()), QString("two"));
        QVERIFY(camera->menu()->actions().last()->isEnabled());
        QCOMPARE(selected.count(), 0);
        view.setInputDevices({}, {}, {}, "two");
        QVERIFY(!camera->isEnabled());
        QCOMPARE(selected.count(), 0);
    }

    void paletteFollowsSystemThemeWithoutStylingWhiteboardChildren()
    {
        CallView view;
        auto     colors = view.palette();
        colors.setColor(QPalette::Window, QColor(QStringLiteral("#212731")));
        colors.setColor(QPalette::WindowText, QColor(QStringLiteral("#f2f5f9")));
        colors.setColor(QPalette::ButtonText, QColor(QStringLiteral("#f2f5f9")));
        colors.setColor(QPalette::Base, QColor(QStringLiteral("#181d24")));
        colors.setColor(QPalette::Highlight, QColor(QStringLiteral("#267fa2")));
        colors.setColor(QPalette::HighlightedText, Qt::white);
        view.setPalette(colors);
        QVERIFY(view.styleSheet().contains(QStringLiteral("#212731")));
        QVERIFY(!view.styleSheet().contains(QStringLiteral("QLabel {")));
        QVERIFY(!view.styleSheet().contains(QStringLiteral("QPushButton {")));
        auto mic = view.findChild<QPushButton *>(QStringLiteral("callMicrophone"));
        QVERIFY(mic && !mic->icon().isNull());
        mic->click();
        QVERIFY(mic->toolTip().contains(QStringLiteral("Unmute")));
    }

    void controlsAndPreview()
    {
        CallView view;
        view.resize(860, 640);
        view.setPeer(QStringLiteral("alex@example.org"));
        view.setInputDevices({ { "mic", "Microphone", true } }, {}, { { "camera", "Camera", true } }, {});
        view.setActive(true);
        view.setDuration(127);
        QSignalSpy microphone(&view, &CallView::microphoneChanged), camera(&view, &CallView::cameraChanged);
        auto       microphoneButton = view.findChild<QPushButton *>(QStringLiteral("callMicrophone"));
        auto       cameraButton     = view.findChild<QPushButton *>(QStringLiteral("callCamera"));
        QVERIFY(microphoneButton && cameraButton);
        microphoneButton->click();
        QCOMPARE(microphone.count(), 1);
        QVERIFY(!view.microphoneEnabled());
        cameraButton->click();
        QCOMPARE(camera.count(), 1);
        QVERIFY(view.cameraEnabled());
        view.setSharing(true, QStringLiteral("Display 1 · 1920 × 1080"));
        view.show();
        QTest::qWait(20);
        QVERIFY(view.grab().save(QDir::temp().filePath(QStringLiteral("psi-screen-call.png"))));
    }

    void mediaControlsAfterWhiteboardConnects()
    {
        CallView view;
        view.setWhiteboardOnly(true);
        view.setMediaAvailable(true);
        view.setVideoSupported(true);
        view.setSharingSupported(true);
        auto mic    = view.findChild<QPushButton *>(QStringLiteral("callMicrophone"));
        auto camera = view.findChild<QPushButton *>(QStringLiteral("callCamera"));
        auto screen = view.findChild<QPushButton *>(QStringLiteral("callShare"));
        QVERIFY(mic && camera && screen);
        QVERIFY(!mic->isEnabled() && !camera->isEnabled());
        QVERIFY(screen->isEnabled()); // Source selection can precede the session.
        view.setConnecting(false);
        view.setActive(true);
        QVERIFY(mic->isEnabled() && camera->isEnabled() && screen->isEnabled());
        QSignalSpy microphone(&view, &CallView::microphoneChanged), video(&view, &CallView::cameraChanged);
        QSignalSpy share(&view, &CallView::shareRequested);
        mic->click();
        camera->click();
        screen->click();
        QCOMPARE(microphone.count(), 1);
        QCOMPARE(video.count(), 1);
        QCOMPARE(share.count(), 1);
        QVERIFY(view.microphoneEnabled() && view.cameraEnabled());
    }

    void screenSelectionBeforeConnection()
    {
        CallView view;
        auto    *screen = view.findChild<QPushButton *>(QStringLiteral("callShare"));
        auto    *hint   = view.findChild<QLabel *>(QStringLiteral("callSharingHint"));
        QVERIFY(screen && hint);
        QVERIFY(!screen->isEnabled()); // A legacy backend has no picker.
        view.setSharingSupported(true);
        QVERIFY(screen->isEnabled());
        QSignalSpy requested(&view, &CallView::shareRequested);
        screen->click();
        QCOMPARE(requested.count(), 1);
        view.setShareSelecting(true);
        view.setConnecting(true);
        QVERIFY(screen->isEnabled());
        QVERIFY(screen->toolTip().contains(QStringLiteral("Cancel")));
        view.setShareSelecting(false);
        view.setSharePrepared(QStringLiteral("Display 1"));
        QVERIFY(screen->isChecked());
        QCOMPARE(hint->toolTip(), QStringLiteral("Screen selected: Display 1"));
        screen->click();
        QCOMPARE(requested.count(), 2); // A pending selection remains cancellable while ringing.
        view.setSharing(false);
        QVERIFY(!screen->isChecked());
        QVERIFY(hint->isHidden());
        view.setSharePrepared(QStringLiteral("Display 2"));
        view.setConnecting(false);
        view.setActive(true);
        view.setSharing(true, QStringLiteral("Display 2"), true);
        QCOMPARE(hint->toolTip(), QStringLiteral("Connecting screen…"));
        view.setSharing(true, QStringLiteral("Display 2"));
        QCOMPARE(hint->toolTip(), QStringLiteral("Sharing: Display 2"));
        view.setFinished();
        QVERIFY(!screen->isEnabled());
        QVERIFY(!screen->isChecked());
        QVERIFY(hint->isHidden());
    }

    void pendingScreenCanBeCancelledAfterCapabilityLoss()
    {
        CallView view;
        auto    *screen = view.findChild<QPushButton *>(QStringLiteral("callShare"));
        QVERIFY(screen);
        view.setSharingSupported(true);
        view.setShareSelecting(true);
        view.setSharingSupported(false);
        view.setMediaAvailable(false);
        QVERIFY(screen->isEnabled());
        view.setShareSelecting(false);
        QVERIFY(!screen->isEnabled());
        view.setSharePrepared(QStringLiteral("Display 1"));
        QVERIFY(screen->isEnabled());
        view.setSharing(false);
        QVERIFY(!screen->isEnabled());
    }
};
QTEST_MAIN(ScreenShareTest)
#include "screenshare.moc"
