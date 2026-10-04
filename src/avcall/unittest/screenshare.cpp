// SPDX-License-Identifier: GPL-2.0-or-later
#include "callview.h"
#include "screensharecapture.h"
#include <QApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <fcntl.h>
#include <unistd.h>

struct PortalStream {
    uint        node;
    QVariantMap properties;
};
using PortalStreams = QList<PortalStream>;
Q_DECLARE_METATYPE(PortalStream)
Q_DECLARE_METATYPE(PortalStreams)
QDBusArgument &operator<<(QDBusArgument &out, const PortalStream &stream)
{
    out.beginStructure();
    out << stream.node << stream.properties;
    out.endStructure();
    return out;
}
const QDBusArgument &operator>>(const QDBusArgument &in, PortalStream &stream)
{
    in.beginStructure();
    in >> stream.node >> stream.properties;
    in.endStructure();
    return in;
}

class PortalSession : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Session")
public:
    int *closed;
    explicit PortalSession(int *count, QObject *parent) : QObject(parent), closed(count) { }
public slots:
    void Close() { ++*closed; }
};

class PortalRequest : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Request")
public:
    int *closed;
    explicit PortalRequest(int *count, QObject *parent) : QObject(parent), closed(count) { }
public slots:
    void Close() { ++*closed; }
};

class Portal : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.ScreenCast")
    Q_PROPERTY(uint AvailableSourceTypes READ sourceTypes)
    Q_PROPERTY(uint AvailableCursorModes READ cursorModes)
public:
    QDBusConnection bus
        = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("screen-test-server"));
    uint                    types = 3, cursors = 2;
    bool                    malformedStreams = false;
    int                     created = 0, sessionsClosed = 0, requestsClosed = 0, delay = 0;
    uint                    response = 0;
    QVariantMap             selection;
    QString                 session;
    QDBusUnixFileDescriptor fd;
    uint                    sourceTypes() const { return types; }
    uint                    cursorModes() const { return cursors; }
    QString                 sender() const
    {
        auto name = message().service().mid(1);
        name.replace(QLatin1Char('.'), QLatin1Char('_'));
        return name;
    }
    QDBusObjectPath respond(const QVariantMap &options, const QVariantMap &results)
    {
        const auto path = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
                              .arg(sender(), options.value(QStringLiteral("handle_token")).toString());
        auto request = new PortalRequest(&requestsClosed, this);
        bus.registerObject(path, request, QDBusConnection::ExportAllSlots);
        const auto code = response;
        QTimer::singleShot(delay, this, [this, path, results, code] {
            auto signal = QDBusMessage::createSignal(path, QStringLiteral("org.freedesktop.portal.Request"),
                                                     QStringLiteral("Response"));
            signal.setArguments({ code, results });
            bus.send(signal);
        });
        return QDBusObjectPath(path);
    }
public slots:
    QDBusObjectPath CreateSession(const QVariantMap &options)
    {
        ++created;
        session = QStringLiteral("/org/freedesktop/portal/desktop/session/%1/%2")
                      .arg(sender(), options.value(QStringLiteral("session_handle_token")).toString());
        auto object = new PortalSession(&sessionsClosed, this);
        bus.registerObject(session, object, QDBusConnection::ExportAllSlots);
        return respond(options, { { QStringLiteral("session_handle"), session } });
    }
    QDBusObjectPath SelectSources(const QDBusObjectPath &, const QVariantMap &options)
    {
        selection = options;
        return respond(options, {});
    }
    QDBusObjectPath Start(const QDBusObjectPath &, const QString &, const QVariantMap &options)
    {
        if (malformedStreams)
            return respond(options, { { QStringLiteral("streams"), QStringLiteral("not a stream array") } });
        return respond(options,
                       { { QStringLiteral("streams"),
                           QVariant::fromValue(
                               PortalStreams { { 73,
                                                 { { QStringLiteral("source_type"), uint(2) },
                                                   { QStringLiteral("pipewire-serial"), qulonglong(9183) } } } }) } });
    }
    QDBusUnixFileDescriptor OpenPipeWireRemote(const QDBusObjectPath &, const QVariantMap &) { return fd; }
};

class ScreenShareTest : public QObject {
    Q_OBJECT
    Portal portal;
private slots:
    void initTestCase()
    {
        qputenv("XDG_SESSION_TYPE", "wayland");
        qDBusRegisterMetaType<PortalStream>();
        qDBusRegisterMetaType<PortalStreams>();
        auto bus = portal.bus;
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), &portal,
                                   QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties));
        int descriptors[2];
        QVERIFY(::pipe(descriptors) == 0);
        portal.fd.setFileDescriptor(descriptors[0]);
        ::close(descriptors[0]);
        ::close(descriptors[1]);
    }
    void init()
    {
        portal.delay    = 0;
        portal.response = 0;
        portal.types    = 3;
        portal.cursors  = 2;
    }

    void cancellingOnSelectionCompletionNeverStartsCapture()
    {
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready), stopped(&capture, &ScreenShareCapture::stopped);
        connect(&capture, &ScreenShareCapture::selectingChanged, &capture, [&](bool selecting) {
            if (!selecting)
                capture.stop();
        });
        const int closed = portal.sessionsClosed;
        capture.select(nullptr);
        QTRY_COMPARE(stopped.count(), 1);
        QTRY_VERIFY(portal.sessionsClosed > closed);
        QCOMPARE(ready.count(), 0);
    }

    void malformedStreamResponseNeverStartsCapture()
    {
        portal.malformedStreams = true;
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready), failed(&capture, &ScreenShareCapture::failed);
        capture.select(nullptr);
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(ready.count(), 0);
        QVERIFY(!capture.selecting());
        portal.malformedStreams = false;
    }

    void windowSelectionAndDescriptorLifetime()
    {
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready), errors(&capture, &ScreenShareCapture::failed);
        capture.select(nullptr);
        QTRY_COMPARE(ready.count(), 1);
        QCOMPARE(errors.count(), 0);
        QCOMPARE(portal.selection.value(QStringLiteral("types")).toUInt(), uint(3));
        QCOMPARE(portal.selection.value(QStringLiteral("cursor_mode")).toUInt(), uint(2));
        QVERIFY(!portal.selection.value(QStringLiteral("multiple")).toBool());
        const auto source = ready.front().front().toString();
        QVERIFY(source.contains(QStringLiteral("target-object=9183")));
        const auto match = QRegularExpression(QStringLiteral("fd=(\\d+)")).match(source);
        QVERIFY(match.hasMatch());
        const int fd = match.captured(1).toInt();
        QVERIFY(fd != portal.fd.fileDescriptor()); // actual Unix-FD passing, not Qt's local-call shortcut
        QVERIFY(fcntl(fd, F_GETFD) != -1);
        bool quiescedWhileFdValid = false;
        connect(&capture, &ScreenShareCapture::stopped, this, [&] { quiescedWhileFdValid = fcntl(fd, F_GETFD) != -1; });
        const int closed = portal.sessionsClosed;
        capture.stop();
        QVERIFY(quiescedWhileFdValid);
        QTRY_VERIFY(portal.sessionsClosed > closed);
        QTRY_VERIFY(fcntl(fd, F_GETFD) == -1);
    }

    void cancellingBeforeCreateResponseClosesSession()
    {
        portal.delay = 50;
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready);
        const int          created = portal.created, closed = portal.sessionsClosed;
        capture.select(nullptr);
        QTRY_VERIFY(portal.created > created);
        capture.stop();
        QTRY_VERIFY(portal.sessionsClosed > closed);
        QTest::qWait(100);
        QCOMPARE(ready.count(), 0);
        QVERIFY(!capture.selecting());
        portal.delay = 0;
        capture.select(nullptr);
        QTRY_COMPARE(ready.count(), 1);
    }

    void portalRevocationStopsConsumer()
    {
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready), stopped(&capture, &ScreenShareCapture::stopped);
        capture.select(nullptr);
        QTRY_COMPARE(ready.count(), 1);
        auto signal = QDBusMessage::createSignal(portal.session, QStringLiteral("org.freedesktop.portal.Session"),
                                                 QStringLiteral("Closed"));
        signal.setArguments({ QVariantMap {} });
        QVERIFY(portal.bus.send(signal));
        QTRY_COMPARE(stopped.count(), 1);
        capture.stop();
        QCOMPARE(stopped.count(), 1);
    }

    void monitorOnlyPortalDoesNotRequestUnsupportedWindowOrCursor()
    {
        portal.types   = 1;
        portal.cursors = 1;
        ScreenShareCapture capture;
        QSignalSpy         ready(&capture, &ScreenShareCapture::ready);
        capture.select(nullptr);
        QTRY_COMPARE(ready.count(), 1);
        QCOMPARE(portal.selection.value(QStringLiteral("types")).toUInt(), uint(1));
        QVERIFY(!portal.selection.contains(QStringLiteral("cursor_mode")));
    }

    void controlsAndPreview()
    {
        CallView view;
        view.resize(860, 640);
        view.setPeer(QStringLiteral("alex@example.org"), false);
        view.setActive(true);
        view.setStatus(QStringLiteral("Connected"));
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
        QVERIFY(view.grab().save(QStringLiteral("/tmp/psi-screen-call.png")));
    }
};
QTEST_MAIN(ScreenShareTest)
#include "screenshare.moc"
