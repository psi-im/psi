#include "x11windowsystem.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h> // needed for WM_CLASS hinting

const long       MAX_PROP_SIZE              = 100000;
X11WindowSystem *X11WindowSystem::_instance = nullptr;

namespace {

bool isPlatformX11()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return QX11Info::isPlatformX11();
#else
    auto x11app = qApp->nativeInterface<QNativeInterface::QX11Application>();
    return !!x11app;
#endif
}

//>>>-- Nathaniel Gray -- Caltech Computer Science ------>
//>>>-- Mojave Project -- http://mojave.cs.caltech.edu -->
// Copied from http://www.nedit.org/archives/discuss/2002-Aug/0386.html

// Helper function
static bool getCardinal32Prop(Display *display, Window win, char *propName, long *value)
{
#if defined(LIMIT_X11_USAGE)
    return false;
#endif

    if (!isPlatformX11()) // Avoid crashes if launched in Wayland
        return false;

    Atom          nameAtom, typeAtom, actual_type_return;
    int           actual_format_return, result;
    unsigned long nitems_return, bytes_after_return;
    long         *result_array = nullptr;

    nameAtom = XInternAtom(display, propName, False);
    typeAtom = XInternAtom(display, "CARDINAL", False);
    if (nameAtom == None || typeAtom == None) {
        // qDebug("Atoms not interned!");
        return false;
    }

    // Try to get the property
    result
        = XGetWindowProperty(display, win, nameAtom, 0, 1, False, typeAtom, &actual_type_return, &actual_format_return,
                             &nitems_return, &bytes_after_return, reinterpret_cast<unsigned char **>(&result_array));

    if (result != Success) {
        // qDebug("not Success");
        return false;
    }
    if (actual_type_return == None || actual_format_return == 0) {
        // qDebug("Prop not found");
        return false;
    }
    if (actual_type_return != typeAtom) {
        // qDebug("Wrong type atom");
    }
    *value = result_array[0];
    XFree(result_array);
    return true;
}

auto getDisplay()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return QX11Info::display();
#else
    auto x11app = qApp->nativeInterface<QNativeInterface::QX11Application>();
    return x11app->display();
#endif
}

auto getRootWindow()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return QX11Info::appRootWindow();
#else
    auto x11app = qApp->nativeInterface<QNativeInterface::QX11Application>();
    return DefaultRootWindow(x11app->display());
#endif
}

}

bool X11WindowSystem::isValid() const { return ::isPlatformX11(); }

void X11WindowSystem::x11wmClass(WId wid, QString resName)
{
#if defined(LIMIT_X11_USAGE)
    return;
#endif

    if (!isValid()) // Avoid crashes if launched in Wayland
        return;

    Display *dsp = getDisplay(); // get the display
    // WId win = winId();                           // get the window
    XClassHint classhint; // class hints
    // Get old class hint. It is important to save old class name
    XGetClassHint(dsp, wid, &classhint);
    XFree(classhint.res_name);

    const QByteArray latinResName = resName.toLatin1();
    classhint.res_name            = const_cast<char *>(latinResName.data()); // res_name
    XSetClassHint(dsp, wid, &classhint);                                     // set the class hints

    XFree(classhint.res_class);
}

void X11WindowSystem::bringToFront(QWidget *w)
{
    if (isValid()) {
        // If we're not on the current desktop, do the hide/show trick
        long   dsk, curr_dsk;
        Window win = w->winId();
        if (X11WindowSystem::instance()->desktopOfWindow(&win, &dsk)
            && X11WindowSystem::instance()->currentDesktop(&curr_dsk)) {
            // qDebug() << "bringToFront current desktop=" << curr_dsk << " windowDesktop=" << dsk;
            if ((dsk != curr_dsk) && (dsk != -1)) { // second condition for sticky windows
                w->hide();
            }
        }
    }

    // FIXME: multi-desktop hacks for Win and Mac required
}

// Get the desktop number that a window is on
bool X11WindowSystem::desktopOfWindow(Window *window, long *desktop)
{
    Display *display = getDisplay();
    bool     result  = getCardinal32Prop(display, *window, const_cast<char *>("_NET_WM_DESKTOP"), desktop);
    // if( result )
    //    qDebug("Desktop: " + QString::number(*desktop));
    return result;
}

// Get the current desktop the WM is displaying
bool X11WindowSystem::currentDesktop(long *desktop)
{
    Window   rootWin;
    Display *display = getDisplay();
    bool     result;

    rootWin = RootWindow(display, XDefaultScreen(display));
    result  = getCardinal32Prop(display, rootWin, const_cast<char *>("_NET_CURRENT_DESKTOP"), desktop);
    // if( result )
    //    qDebug("Current Desktop: " + QString::number(*desktop));
    return result;
}

X11WindowSystem::X11WindowSystem()
{
    const int   atomsCount        = 10;
    const char *names[atomsCount] = { "_NET_CLIENT_LIST_STACKING",
                                      "_NET_FRAME_EXTENTS",

                                      "_NET_WM_WINDOW_TYPE",
                                      "_NET_WM_WINDOW_TYPE_NORMAL",
                                      "_NET_WM_WINDOW_TYPE_DIALOG",
                                      "_NET_WM_WINDOW_TYPE_UTILITY",
                                      "_NET_WM_WINDOW_TYPE_SPLASH",

                                      "_NET_WM_STATE",
                                      "_NET_WM_STATE_ABOVE",
                                      "_NET_WM_STATE_HIDDEN" };
    Atom        atoms[atomsCount],
        *atomsp[atomsCount] = { &net_client_list_stacking,
                                &net_frame_extents,

                                &net_wm_window_type,
                                &net_wm_window_type_normal,
                                &net_wm_window_type_dialog,
                                &net_wm_window_type_utility,
                                &net_wm_window_type_splash,

                                &net_wm_state,
                                &net_wm_state_above,
                                &net_wm_state_hidden };
    int i                   = atomsCount;
    while (i--)
        atoms[i] = 0;

#if !defined(LIMIT_X11_USAGE)
    XInternAtoms(getDisplay(), const_cast<char **>(names), atomsCount, true, atoms);
#endif

    i = atomsCount;
    while (i--)
        *atomsp[i] = atoms[i];

    if (net_wm_window_type_normal != None)
        normalWindows.insert(net_wm_window_type_normal);
    if (net_wm_window_type_dialog != None)
        normalWindows.insert(net_wm_window_type_dialog);
    if (net_wm_window_type_utility != None)
        normalWindows.insert(net_wm_window_type_utility);
    if (net_wm_window_type_splash != None)
        normalWindows.insert(net_wm_window_type_splash);

    if (net_wm_state_hidden != None)
        ignoredWindowStates.insert(net_wm_state_hidden);
}

X11WindowSystem &X11WindowSystem::operator=(const X11WindowSystem &) { return *this; }

X11WindowSystem *X11WindowSystem::instance()
{
    if (!_instance)
        _instance = new X11WindowSystem();
    return X11WindowSystem::_instance;
}

// Get window coords relative to desktop window
QRect X11WindowSystem::windowRect(Window win)
{
    Window       w_unused;
    int          x, y;
    unsigned int w, h, junk;
    Display     *display = getDisplay();
    XGetGeometry(display, win, &w_unused, &x, &y, &w, &h, &junk, &junk);
    XTranslateCoordinates(display, win, getRootWindow(), 0, 0, &x, &y, &w_unused);

    Atom           type_ret;
    int            format_ret;
    unsigned char *data_ret;
    unsigned long  nitems_ret, unused;
    const Atom     XA_CARDINAL = Atom(6);
    if (net_frame_extents != None
        && XGetWindowProperty(display, win, net_frame_extents, 0l, 4l, False, XA_CARDINAL, &type_ret, &format_ret,
                              &nitems_ret, &unused, &data_ret)
            == Success) {
        if (type_ret == XA_CARDINAL && format_ret == 32 && nitems_ret == 4) {
            // Struts array: 0 - left, 1 - right, 2 - top, 3 - bottom
            long *d = reinterpret_cast<long *>(data_ret);
            x -= d[0];
            y -= d[2];
            w += d[0] + d[1];
            h += d[2] + d[3];
        }
        if (data_ret)
            XFree(data_ret);
    }

    return { x, y, int(w), int(h) };
}

// Determine if window is obscured by other windows
bool X11WindowSystem::isWindowObscured(QWidget *widget, bool alwaysOnTop)
{
    Display *display = getDisplay();
    if (net_wm_state_above != None) {
        if (!alwaysOnTop)
            ignoredWindowStates.insert(net_wm_state_above);
        else
            ignoredWindowStates.remove(net_wm_state_above);
    }

    // TODO Is it correct to use QX11Info::appRootWindow() as root window?
    Q_ASSERT(widget);
    QWidget *w   = widget->window();
    Window   win = w->winId();
    long     desktop;
    desktopOfWindow(&win, &desktop);

    const Atom     XA_WINDOW = Atom(33);
    Atom           type_ret;
    int            format_ret;
    unsigned char *data_ret;
    unsigned long  nitems_ret, unused;

    if (net_client_list_stacking != None) {
        QRect winRect = windowRect(win);
        if (XGetWindowProperty(display, getRootWindow(), net_client_list_stacking, 0, MAX_PROP_SIZE, False, XA_WINDOW,
                               &type_ret, &format_ret, &nitems_ret, &unused, &data_ret)
            == Success) {
            if (type_ret == XA_WINDOW && format_ret == 32) {
                Window *wins = reinterpret_cast<Window *>(data_ret);

                // Enumerate windows in reverse order (from most foreground window)
                while (nitems_ret--) {
                    Window current = wins[nitems_ret];

                    long winDesktop;
                    desktopOfWindow(&current, &winDesktop);
                    if (desktop != winDesktop)
                        continue;

                    // We are not interested in underlying windows
                    if (current == win)
                        break;

                    // If our window in not alwaysOnTop ignore such windows, because we can't raise on top of them
                    if (windowHasAnyOfStates(current, ignoredWindowStates))
                        continue;

                    if (!windowHasOnlyTypes(current, normalWindows))
                        continue;

                    QRect rect = windowRect(current);
                    if (winRect.intersects(rect)) {
                        XFree(data_ret);
                        return true;
                    }
                }
            }
            if (data_ret)
                XFree(data_ret);
        }
    }
    return false;
}

// If window has any type other than allowed_types return false, else return true
bool X11WindowSystem::windowHasOnlyTypes(Window win, const QSet<Atom> &allowedTypes)
{
    const Atom     XA_ATOM = Atom(4);
    Atom           type_ret;
    int            format_ret;
    unsigned char *data_ret;
    unsigned long  nitems_ret, unused;
    Display       *display = getDisplay();

    if (net_wm_window_type != None
        && XGetWindowProperty(display, win, net_wm_window_type, 0l, 2048l, False, XA_ATOM, &type_ret, &format_ret,
                              &nitems_ret, &unused, &data_ret)
            == Success) {
        if (type_ret == XA_ATOM && format_ret == 32 && nitems_ret > 0) {
            Atom *types = reinterpret_cast<Atom *>(data_ret);
            for (unsigned long i = 0; i < nitems_ret; i++) {
                if (!allowedTypes.contains(types[i])) {
                    return false;
                }
            }
        }
        if (data_ret)
            XFree(data_ret);
        return true;
    } else
        return false;
}

// If window has any of filteredStates return
bool X11WindowSystem::windowHasAnyOfStates(Window win, const QSet<Atom> &filteredStates)
{
    const Atom     XA_ATOM = Atom(4);
    Atom           type_ret;
    int            format_ret;
    unsigned char *data_ret;
    unsigned long  nitems_ret, unused;
    Display       *display = getDisplay();

    if (net_wm_state != None
        && XGetWindowProperty(display, win, net_wm_state, 0l, 2048l, False, XA_ATOM, &type_ret, &format_ret,
                              &nitems_ret, &unused, &data_ret)
            == Success) {
        if (type_ret == XA_ATOM && format_ret == 32 && nitems_ret > 0) {
            Atom *states = reinterpret_cast<Atom *>(data_ret);
            for (unsigned long i = 0; i < nitems_ret; i++) {

                if (filteredStates.contains(states[i]))
                    return true;
            }
        }
        if (data_ret)
            XFree(data_ret);
    }
    return false;
}

ulong X11WindowSystem::getDesktopRootWindow() { return getRootWindow(); };
