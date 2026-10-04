// SPDX-License-Identifier: GPL-2.0-or-later
#include "screensharecapture.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QTabWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <atomic>
#include <utility>
#ifdef PSI_SCREENSHARE_PORTAL
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif
#ifdef PSI_SCREENSHARE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#ifdef PSI_SCREENSHARE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
// Xlib's macros collide with Qt enum names.
#undef None
#endif

namespace {
#ifdef PSI_SCREENSHARE_X11
// Windows may disappear between listing and querying them. Contain X errors
// on our separate enumeration connection instead of invoking Xlib's fatal
// default handler. Errors on other connections retain their existing handler.
class X11EnumerationErrors {
    static inline std::atomic<Display *>     display_  = nullptr;
    static inline std::atomic<XErrorHandler> previous_ = nullptr;
    static int                               handle(Display *display, XErrorEvent *event)
    {
        if (display == display_)
            return 0;
        const auto previous = previous_.load();
        return previous ? previous(display, event) : 0;
    }

public:
    explicit X11EnumerationErrors(Display *display)
    {
        display_  = display;
        previous_ = XSetErrorHandler(handle);
    }
    ~X11EnumerationErrors()
    {
        XSync(display_.load(), False);
        XSetErrorHandler(previous_.load());
        display_ = nullptr;
    }
};
#endif
bool waylandSession()
{
    return QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))
        || qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland");
}
#ifdef PSI_SCREENSHARE_PORTAL
const QString portalService       = QStringLiteral("org.freedesktop.portal.Desktop");
const QString portalPath          = QStringLiteral("/org/freedesktop/portal/desktop");
const QString screenCastInterface = QStringLiteral("org.freedesktop.portal.ScreenCast");
QString       token() { return QStringLiteral("psi_") + QUuid::createUuid().toString(QUuid::Id128); }
void          closePortalObject(const QString &path, const QString &interface)
{
    if (path.isEmpty())
        return;
    QDBusConnection::sessionBus().asyncCall(
        QDBusMessage::createMethodCall(portalService, path, interface, QStringLiteral("Close")));
}
#endif
}

ScreenShareCapture::ScreenShareCapture(QObject *parent) : QObject(parent) { }
ScreenShareCapture::~ScreenShareCapture() { stop(); }
bool ScreenShareCapture::selecting() const { return selecting_; }

bool ScreenShareCapture::available()
{
#ifdef PSI_SCREENSHARE_PORTAL
    if (waylandSession())
        return QDBusConnection::sessionBus().isConnected();
#endif
#ifdef PSI_SCREENSHARE_X11
    return !waylandSession() && QGuiApplication::platformName() == QLatin1String("xcb");
#else
    return false;
#endif
}

void ScreenShareCapture::select(QWidget *parent)
{
    QPointer<ScreenShareCapture> guard(this);
    stop();
    if (!guard)
        return;
    selecting_                     = true;
    const auto selectionGeneration = generation_;
    emit       selectingChanged(true);
    if (!guard || generation_ != selectionGeneration || !selecting_)
        return;
#ifdef PSI_SCREENSHARE_PORTAL
    if (waylandSession()) {
        // An empty parent is permitted by the portal. X11 IDs must never be
        // passed for Wayland windows; exporting a Wayland handle is optional.
        parentWindow_.clear();
        step_      = Step::Create;
        auto query = QDBusMessage::createMethodCall(
            portalService, portalPath, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
        query.setArguments({ screenCastInterface });
        const auto generation = generation_;
        auto       watcher    = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(query), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, generation](QDBusPendingCallWatcher *watcher) {
                    const QDBusPendingReply<QVariantMap> reply = *watcher;
                    watcher->deleteLater();
                    if (generation != generation_)
                        return;
                    if (reply.isError()) {
                        fail(tr("Screen sharing requires a working desktop ScreenCast portal."));
                        return;
                    }
                    sourceTypes_ = reply.value().value(QStringLiteral("AvailableSourceTypes")).toUInt() & 3;
                    embedCursor_ = reply.value().value(QStringLiteral("AvailableCursorModes")).toUInt() & 2;
                    if (!sourceTypes_) {
                        fail(tr("This desktop does not provide display or window capture."));
                        return;
                    }
                    const auto sessionToken = token();
                    auto       bus          = QDBusConnection::sessionBus();
                    auto       sender       = bus.baseService().mid(1);
                    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
                    sessionPath_
                        = QStringLiteral("/org/freedesktop/portal/desktop/session/%1/%2").arg(sender, sessionToken);
                    bus.connect(portalService, sessionPath_, QStringLiteral("org.freedesktop.portal.Session"),
                                QStringLiteral("Closed"), this, SLOT(portalClosed(QVariantMap)));
                    requestPortal(QStringLiteral("CreateSession"), {},
                                  { { QStringLiteral("session_handle_token"), sessionToken } });
                });
        return;
    }
#endif
    selectX11(parent);
}

void ScreenShareCapture::stop()
{
    QPointer<ScreenShareCapture> guard(this);
    ++generation_;
    const bool wasActive    = std::exchange(active_, false);
    const bool wasSelecting = std::exchange(selecting_, false);
    // Detach old resources before notifying consumers: their callbacks may
    // destroy this object or start another selection synchronously.
    auto lease = std::move(sourceLease_);
#ifdef PSI_SCREENSHARE_PORTAL
    auto       bus        = QDBusConnection::sessionBus();
    const auto request    = std::exchange(requestPath_, {});
    const auto session    = std::exchange(sessionPath_, {});
    const auto descriptor = std::exchange(pipeWireFd_, {});
    step_                 = Step::Idle;
    if (!request.isEmpty()) {
        bus.disconnect(portalService, request, QStringLiteral("org.freedesktop.portal.Request"),
                       QStringLiteral("Response"), this, SLOT(portalResponse(uint, QVariantMap, QDBusMessage)));
        closePortalObject(request, QStringLiteral("org.freedesktop.portal.Request"));
    }
    if (!session.isEmpty())
        bus.disconnect(portalService, session, QStringLiteral("org.freedesktop.portal.Session"),
                       QStringLiteral("Closed"), this, SLOT(portalClosed(QVariantMap)));
#endif
    if (picker_) {
        auto picker = picker_;
        picker_.clear();
        picker->disconnect(this);
        picker->reject();
    }
    if (wasActive)
        emit stopped(); // old FD stays valid through synchronous capture revocation
#ifdef PSI_SCREENSHARE_PORTAL
    closePortalObject(session, QStringLiteral("org.freedesktop.portal.Session"));
#endif
    if (guard && wasSelecting && !selecting_)
        emit selectingChanged(false);
}

void ScreenShareCapture::fail(const QString &message)
{
    QPointer<ScreenShareCapture> guard(this);
    stop();
    if (guard)
        emit failed(message);
}

void ScreenShareCapture::selectX11(QWidget *parent)
{
#ifdef PSI_SCREENSHARE_X11
    if (!available()) {
        fail(tr("Screen sharing is unavailable on this desktop."));
        return;
    }
    // Do not share Qt's X connection with either the picker or GStreamer.
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        fail(tr("Unable to connect to the display."));
        return;
    }
    auto dialog = new QDialog(parent);
    picker_     = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Choose what to share"));
    dialog->setStyleSheet(QStringLiteral(
        "QDialog { background: #111827; color: #f3f4f6; }"
        "QLabel { color: #f3f4f6; }"
        "QTabWidget::pane { border: none; }"
        "QTabBar::tab { background: #293447; color: #d1d5db; padding: 10px 20px; border-radius: 6px; margin: 4px; }"
        "QTabBar::tab:selected { background: #145c55; color: #f3f4f6; }"
        "QListWidget { background: #1b2435; color: #f3f4f6; border: none; border-radius: 12px; padding: 8px; }"
        "QListWidget::item:selected { background: #145c55; border: 2px solid #2dd4bf; border-radius: 8px; }"
        "QPushButton { background: #293447; color: #f3f4f6; border: 1px solid #39465c; border-radius: 8px; padding: "
        "10px 24px; }"
        "QPushButton:hover { background: #145c55; }"
        "QPushButton:disabled { color: #718096; }"));
    dialog->resize(760, 510);
    auto layout  = new QVBoxLayout(dialog);
    auto heading = new QLabel(tr("Share a display or a single window"), dialog);
    heading->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 600; padding: 12px;"));
    layout->addWidget(heading);
    auto tabs = new QTabWidget(dialog);
    layout->addWidget(tabs, 1);
    auto screens = new QListWidget(tabs);
    auto windows = new QListWidget(tabs);
    for (auto list : { screens, windows }) {
        list->setViewMode(QListView::IconMode);
        list->setIconSize(QSize(210, 125));
        list->setGridSize(QSize(230, 165));
        list->setResizeMode(QListView::Adjust);
        list->setMovement(QListView::Static);
        list->setSpacing(8);
    }
    tabs->addTab(screens, tr("Displays"));
    tabs->addTab(windows, tr("Windows"));
    {
        X11EnumerationErrors errors(display);
        const Window         root = DefaultRootWindow(display);
        const auto append = [](QListWidget *list, const QString &name, const QString &source, const QPixmap &preview) {
            auto item = new QListWidgetItem(QIcon(preview), name, list);
            item->setData(Qt::UserRole, source);
            item->setToolTip(QStringLiteral("<qt>%1</qt>").arg(name.toHtmlEscaped()));
        };
        // XRandR supplies actual root pixel coordinates, including mixed-DPI
        // displays. Qt's logical screen geometry cannot be used as capture pixels.
        QList<QPair<QString, QRect>> monitors;
#ifdef PSI_SCREENSHARE_XRANDR
        if (auto resources = XRRGetScreenResourcesCurrent(display, root)) {
            for (int i = 0; i < resources->noutput; ++i) {
                auto output = XRRGetOutputInfo(display, resources, resources->outputs[i]);
                if (!output)
                    continue;
                if (output->connection == RR_Connected && output->crtc) {
                    auto          crtc = XRRGetCrtcInfo(display, resources, output->crtc);
                    const QString name = QString::fromUtf8(output->name, output->nameLen);
                    if (crtc && crtc->width && crtc->height) {
                        monitors.append({ name, QRect(crtc->x, crtc->y, int(crtc->width), int(crtc->height)) });
                    }
                    if (crtc)
                        XRRFreeCrtcInfo(crtc);
                }
                XRRFreeOutputInfo(output);
            }
            XRRFreeScreenResources(resources);
        }
#else
        // Keep X11 sharing buildable without the optional XRandR development
        // package. Its standard utility reports the same native pixel rectangles.
        QProcess query;
        query.start(QStringLiteral("xrandr"), { QStringLiteral("--listmonitors") });
        if (query.waitForFinished(500)) {
            const QRegularExpression geometry(QStringLiteral(
                "^\\s*\\d+:\\s+\\S+\\s+(\\d+)(?:/\\d+)?x(\\d+)(?:/\\d+)?([+-]\\d+)([+-]\\d+)\\s+(\\S+)"));
            for (const auto &line : QString::fromUtf8(query.readAllStandardOutput()).split(QLatin1Char('\n'))) {
                const auto match = geometry.match(line);
                if (match.hasMatch())
                    monitors.append({ match.captured(5),
                                      QRect(match.captured(3).toInt(), match.captured(4).toInt(),
                                            match.captured(1).toInt(), match.captured(2).toInt()) });
            }
        } else {
            query.kill();
            query.waitForFinished(100);
        }
#endif
        if (monitors.isEmpty()) {
            XWindowAttributes attributes;
            if (XGetWindowAttributes(display, root, &attributes))
                monitors.append({ tr("Desktop"), QRect(0, 0, attributes.width, attributes.height) });
        }
        for (const auto &monitor : monitors) {
            const auto &rect = monitor.second;
            if (rect.isEmpty() || rect.x() < 0 || rect.y() < 0)
                continue;
            const auto source
                = QStringLiteral("ximagesrc use-damage=false show-pointer=true startx=%1 starty=%2 endx=%3 endy=%4")
                      .arg(rect.x())
                      .arg(rect.y())
                      .arg(rect.right())
                      .arg(rect.bottom());
            QPixmap preview;
            for (auto screen : QGuiApplication::screens()) {
                if (screen->name() == monitor.first) {
                    preview = screen->grabWindow(0).scaled(210, 125, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    break;
                }
            }
            append(screens, tr("%1 · %2 × %3").arg(monitor.first).arg(rect.width()).arg(rect.height()), source,
                   preview);
        }
        Atom           actualType;
        int            actualFormat;
        unsigned long  count, remaining;
        unsigned char *data = nullptr;
        if (XGetWindowProperty(display, root, XInternAtom(display, "_NET_CLIENT_LIST_STACKING", False), 0, 65536, False,
                               XA_WINDOW, &actualType, &actualFormat, &count, &remaining, &data)
                == Success
            && actualType == XA_WINDOW && actualFormat == 32 && data) {
            auto ids = reinterpret_cast<Window *>(data);
            for (unsigned long i = 0; i < count; ++i) {
                XWindowAttributes attributes;
                if ((parent && ids[i] == Window(parent->winId())) || !XGetWindowAttributes(display, ids[i], &attributes)
                    || attributes.map_state != IsViewable)
                    continue;
                unsigned char *name = nullptr;
                unsigned long  length, rest;
                if (XGetWindowProperty(display, ids[i], XInternAtom(display, "_NET_WM_NAME", False), 0, 4096, False,
                                       XInternAtom(display, "UTF8_STRING", False), &actualType, &actualFormat, &length,
                                       &rest, &name)
                        != Success
                    || !name)
                    continue;
                const QString title
                    = actualFormat == 8 ? QString::fromUtf8(reinterpret_cast<char *>(name), int(length)) : QString();
                XFree(name);
                if (title.isEmpty())
                    continue;
                const QPixmap preview = QGuiApplication::primaryScreen()
                                            ->grabWindow(WId(ids[i]))
                                            .scaled(210, 125, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                append(windows, title,
                       QStringLiteral("ximagesrc use-damage=false show-pointer=true xid=%1").arg(qulonglong(ids[i])),
                       preview);
            }
        }
        if (data)
            XFree(data);
    }
    XCloseDisplay(display);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Share"));
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    layout->addWidget(buttons);
    for (auto list : { screens, windows }) {
        connect(list, &QListWidget::itemSelectionChanged, dialog, [tabs, buttons] {
            auto current = qobject_cast<QListWidget *>(tabs->currentWidget());
            buttons->button(QDialogButtonBox::Ok)->setEnabled(current && current->currentItem());
        });
        connect(list, &QListWidget::itemDoubleClicked, dialog, [dialog](QListWidgetItem *) { dialog->accept(); });
    }
    connect(tabs, &QTabWidget::currentChanged, dialog, [tabs, buttons] {
        auto current = qobject_cast<QListWidget *>(tabs->currentWidget());
        buttons->button(QDialogButtonBox::Ok)->setEnabled(current && current->currentItem());
    });
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(dialog, &QDialog::accepted, this, [this, tabs] {
        auto current = qobject_cast<QListWidget *>(tabs->currentWidget());
        if (!current || !current->currentItem()) {
            stop();
            return;
        }
        const auto source = current->currentItem()->data(Qt::UserRole).toString();
        const auto label  = current->currentItem()->text();
        picker_.clear();
        selecting_ = false;
        active_    = true;
        QPointer<ScreenShareCapture> guard(this);
        const auto                   generation = generation_;
        emit                         selectingChanged(false);
        if (guard && active_ && generation == generation_)
            emit ready(source, label);
    });
    connect(dialog, &QDialog::rejected, this, [this] {
        picker_.clear();
        stop();
    });
    dialog->open();
#else
    Q_UNUSED(parent)
    fail(tr("Screen sharing is unavailable on this desktop."));
#endif
}

#ifdef PSI_SCREENSHARE_PORTAL
void ScreenShareCapture::requestPortal(const QString &method, const QVariantList &arguments, QVariantMap options)
{
    auto          bus    = QDBusConnection::sessionBus();
    const QString handle = token();
    QString       sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    requestPath_ = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, handle);
    options.insert(QStringLiteral("handle_token"), handle);
    if (!bus.connect(portalService, requestPath_, QStringLiteral("org.freedesktop.portal.Request"),
                     QStringLiteral("Response"), this, SLOT(portalResponse(uint, QVariantMap, QDBusMessage)))) {
        fail(tr("Unable to open the screen sharing dialog."));
        return;
    }
    auto message      = QDBusMessage::createMethodCall(portalService, portalPath, screenCastInterface, method);
    auto allArguments = arguments;
    allArguments.append(options);
    message.setArguments(allArguments);
    const auto generation = generation_;
    auto       watcher    = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *watcher) {
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (generation != generation_)
            return;
        if (reply.isError())
            fail(tr("Unable to open the screen sharing dialog: %1").arg(reply.error().message()));
    });
}

void ScreenShareCapture::portalResponse(uint response, const QVariantMap &results, const QDBusMessage &message)
{
    if (message.path() != requestPath_ || step_ == Step::Idle)
        return;
    auto bus = QDBusConnection::sessionBus();
    bus.disconnect(portalService, requestPath_, QStringLiteral("org.freedesktop.portal.Request"),
                   QStringLiteral("Response"), this, SLOT(portalResponse(uint, QVariantMap, QDBusMessage)));
    requestPath_.clear();
    if (response != 0) {
        if (response == 1)
            stop();
        else
            fail(tr("The desktop could not start screen sharing."));
        return;
    }
    if (step_ == Step::Create) {
        if (results.value(QStringLiteral("session_handle")).toString() != sessionPath_) {
            fail(tr("The desktop returned an invalid capture session."));
            return;
        }
        step_ = Step::Select;
        QVariantMap options { { QStringLiteral("types"), sourceTypes_ }, { QStringLiteral("multiple"), false } };
        if (embedCursor_)
            options.insert(QStringLiteral("cursor_mode"), uint(2));
        requestPortal(QStringLiteral("SelectSources"), { QVariant::fromValue(QDBusObjectPath(sessionPath_)) }, options);
    } else if (step_ == Step::Select) {
        step_ = Step::Start;
        requestPortal(QStringLiteral("Start"), { QVariant::fromValue(QDBusObjectPath(sessionPath_)), parentWindow_ },
                      {});
    } else if (step_ == Step::Start) {
        const auto streams = results.value(QStringLiteral("streams"));
        if (streams.userType() != qMetaTypeId<QDBusArgument>()) {
            fail(tr("The desktop returned no capture stream."));
            return;
        }
        const auto argument = qvariant_cast<QDBusArgument>(streams);
        if (argument.currentSignature() != QLatin1String("a(ua{sv})")) {
            fail(tr("The desktop returned an invalid capture stream."));
            return;
        }
        uint        node = 0;
        QVariantMap properties;
        argument.beginArray();
        if (!argument.atEnd()) {
            argument.beginStructure();
            argument >> node >> properties;
            argument.endStructure();
        }
        const bool exactlyOne = argument.atEnd();
        argument.endArray();
        if (!node || !exactlyOne) {
            fail(tr("The desktop returned an invalid capture stream."));
            return;
        }
        openPipeWire(node, properties);
    }
}

void ScreenShareCapture::openPipeWire(uint node, const QVariantMap &properties)
{
    step_        = Step::Open;
    auto message = QDBusMessage::createMethodCall(portalService, portalPath, screenCastInterface,
                                                  QStringLiteral("OpenPipeWireRemote"));
    message.setArguments({ QVariant::fromValue(QDBusObjectPath(sessionPath_)), QVariantMap {} });
    const auto generation = generation_;
    auto       watcher    = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, node, properties, generation](QDBusPendingCallWatcher *watcher) {
                const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher;
                watcher->deleteLater();
                if (generation != generation_ || step_ != Step::Open)
                    return;
                if (reply.isError() || !reply.value().isValid()) {
                    fail(tr("Unable to access the selected display or window."));
                    return;
                }
                pipeWireFd_       = reply.value();
                sourceLease_      = std::make_shared<QDBusUnixFileDescriptor>(pipeWireFd_);
                const auto serial = properties.value(QStringLiteral("pipewire-serial")).toULongLong();
                const auto target
                    = serial ? QStringLiteral("target-object=%1").arg(serial) : QStringLiteral("path=%1").arg(node);
                const auto source = QStringLiteral("pipewiresrc fd=%1 %2 do-timestamp=true")
                                        .arg(pipeWireFd_.fileDescriptor())
                                        .arg(target);
                step_      = Step::Idle;
                selecting_ = false;
                active_    = true;
                QPointer<ScreenShareCapture> guard(this);
                emit                         selectingChanged(false);
                if (!guard || !active_ || generation != generation_)
                    return;
                emit ready(source,
                           properties.value(QStringLiteral("source_type")).toUInt() == 2 ? tr("Selected window")
                                                                                         : tr("Selected display"));
            });
}

void ScreenShareCapture::portalClosed(const QVariantMap &)
{
    sessionPath_.clear(); // the portal has already closed it
    stop();
}
#endif
