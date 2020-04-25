#ifndef X11WINDOWSYSTEM_H
#define X11WINDOWSYSTEM_H

#include <QRect>
#include <QSet>
#include <QWidget>
#include <QX11Info>
#include <QtCore>

// TODO: Find a way to include Xlib here and not redefine Atom and Window types
typedef unsigned long Atom;
typedef unsigned long Window;

class X11WindowSystem {
private:
    Atom net_client_list_stacking = 0ul;
    Atom net_frame_extents        = 0ul;

    Atom net_wm_window_type         = 0ul;
    Atom net_wm_window_type_normal  = 0ul;
    Atom net_wm_window_type_dialog  = 0ul;
    Atom net_wm_window_type_utility = 0ul;
    Atom net_wm_window_type_splash  = 0ul;

    Atom net_wm_state        = 0ul;
    Atom net_wm_state_above  = 0ul;
    Atom net_wm_state_hidden = 0ul;

    QSet<Atom> normalWindows, ignoredWindowStates;

    static X11WindowSystem *_instance;
    X11WindowSystem();
    ~X11WindowSystem() { }
    X11WindowSystem(const X11WindowSystem &) = default;
    X11WindowSystem &operator                =(const X11WindowSystem &);

public:
    static X11WindowSystem *instance();
    QRect                   windowRect(Window win);
    bool                    isWindowObscured(QWidget *widget, bool alwaysOnTop);
    bool                    windowHasOnlyTypes(Window win, const QSet<Atom> &allowedTypes);
    bool                    windowHasAnyOfStates(Window win, const QSet<Atom> &filteredStates);
    bool                    currentDesktop(long *desktop);
    bool                    desktopOfWindow(Window *window, long *desktop);
    void                    x11wmClass(WId wid, QString resName);
};

#endif // X11WINDOWSYSTEM_H
