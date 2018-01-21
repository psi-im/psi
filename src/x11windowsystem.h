#ifndef X11WINDOWSYSTEM_H
#define X11WINDOWSYSTEM_H

#include <QtCore>
#include <QWidget>
#include <QRect>
#include <QSet>
#include <QX11Info>

//TODO: Find a way to include Xlib here and not redefine Atom and Window types
typedef unsigned long Atom;
typedef unsigned long Window;

class X11WindowSystem {
private:
    Atom net_client_list_stacking;
    Atom net_frame_extents;

    Atom net_wm_window_type;
    Atom net_wm_window_type_normal;
    Atom net_wm_window_type_dialog;
    Atom net_wm_window_type_utility;
    Atom net_wm_window_type_splash;

    Atom net_wm_state;
    Atom net_wm_state_above;
    Atom net_wm_state_hidden;

    QSet<Atom> normalWindows, ignoredWindowStates;

    static X11WindowSystem* _instance;
    X11WindowSystem();
    ~X11WindowSystem() {}
    X11WindowSystem(const X11WindowSystem& ) {};
    X11WindowSystem & operator=(const X11WindowSystem &) {return *this;};

public:
    static X11WindowSystem* instance();
    QRect windowRect(Window win);
    bool isWindowObscured(QWidget *widget, bool alwaysOnTop);
    bool windowHasOnlyTypes(Window win, const QSet<Atom> &allowedTypes);
    bool windowHasAnyOfStates(Window win, const QSet<Atom> &filteredStates);
    bool currentDesktop(long *desktop);
    bool desktopOfWindow(Window *window, long *desktop);
    void x11wmClass(WId wid, QString resName);
};

#endif // X11WINDOWSYSTEM_H
