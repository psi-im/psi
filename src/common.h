/*
 * common.h - contains all the common variables and functions for Psi
 * Copyright (C) 2001-2003  Justin Karneges
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include "statuspreset.h"
#include "xmpp_status.h"

#include <QColor>
#include <QGridLayout>
#include <QList>
#include <QMap>
#include <QSize>
#include <QString>
#include <QStringList>

#if __cplusplus < 201703L
#define PSI_FALLSTHROUGH
#else
#define PSI_FALLSTHROUGH [[fallthrough]]
#endif

class QMenu;
class TabbableWidget;

// -----------------------------------------------------------------------------
// Options
// -----------------------------------------------------------------------------

enum { dcClose, dcHour, dcDay, dcNever };

enum Qt3Dock {
    Qt3Dock_Unmanaged = 0,
    Qt3Dock_TornOff   = 1,
    Qt3Dock_Top       = 2,
    Qt3Dock_Bottom    = 3,
    Qt3Dock_Right     = 4,
    Qt3Dock_Left      = 5,
    Qt3Dock_Minimized = 6
};

class ToolbarPrefs {
public:
    ToolbarPrefs();

    QString     id;
    QString     name;
    QStringList keys;
    Qt3Dock     dock;

    // bool dirty;
    bool on;
    bool locked;
    // bool stretchable;

    // int index;
    bool nl;
    // int extraOffset;

    bool operator==(const ToolbarPrefs &other);
};

struct lateMigrationOptions {

    QMap<QString, QString> serviceRosterIconset;
    QMap<QString, QString> customRosterIconset;

    QMap<QString, StatusPreset> sp; // Status message presets.

    QMap<QString, QList<ToolbarPrefs>> toolbars;
};

// used to be part of the global options struct.
// do not modify or save/load this value! it is calculated at run time!
// FIXME find it a new home!
extern int common_smallFontSize;

// used to be part of the global options struct.
// FIXME find it a new home!
enum { EventPriorityDontCare = -1 };

// -----------------------------------------------------------------------------
// Status
// -----------------------------------------------------------------------------

#define STATUS_OFFLINE XMPP::Status::Offline
#define STATUS_ONLINE XMPP::Status::Online
#define STATUS_AWAY XMPP::Status::Away
#define STATUS_XA XMPP::Status::XA
#define STATUS_DND XMPP::Status::DND
#define STATUS_INVISIBLE XMPP::Status::Invisible
#define STATUS_CHAT XMPP::Status::FFC

#define STATUS_ASK 100
#define STATUS_NOAUTH 101
#define STATUS_ERROR 102

QString            status2txt(int status);
bool               lastPriorityNotEmpty();
XMPP::Status       makeLastStatus(int);
XMPP::Status       makeStatus(int, const QString &);
XMPP::Status       makeStatus(int, const QString &, int);
XMPP::Status::Type makeSTATUS(const XMPP::Status &);
QString            clipStatus(const QString &str, int width, int height);
inline int         rankStatus(int status)
{
    switch (status) {
    case XMPP::Status::FFC:
        return 0;
    case XMPP::Status::Online:
        return 1;
    case XMPP::Status::Away:
        return 2;
    case XMPP::Status::XA:
        return 3;
    case XMPP::Status::DND:
        return 4;
    case XMPP::Status::Invisible:
        return 5;
    default:
        return 6;
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Widget tools
// -----------------------------------------------------------------------------

bool            isKde();
void            clearMenu(QMenu *m); // deletes all items, including submenus, from given QMenu
void            bringToFront(QWidget *w, bool grabFocus = true);
void            replaceWidget(QWidget *, QWidget *);
void            closeDialogs(QWidget *);
TabbableWidget *findActiveTab();
#ifdef HAVE_X11
#include "x11windowsystem.h"
#define X11WM_CLASS(x) X11WindowSystem::instance()->x11wmClass(winId(), (x));
#else
#define X11WM_CLASS(x) /* dummy */
#endif
void reorderGridLayout(QGridLayout *layout, int maxCols);

// -----------------------------------------------------------------------------
// History utilities
// -----------------------------------------------------------------------------

QString logencode(QString);
QString logdecode(const QString &);

// -----------------------------------------------------------------------------
// Misc.
// -----------------------------------------------------------------------------

QString CAP(const QString &str);

QString encodePassword(const QString &, const QString &);
QString decodePassword(const QString &, const QString &);
#ifdef HAVE_KEYCHAIN
void saveXMPPPasswordToKeyring(const QString &jid, const QString &pass, QObject *parent);
bool isKeychainEnabled();
#endif

bool operator!=(const QMap<QString, QString> &, const QMap<QString, QString> &);

bool fileCopy(const QString &src, const QString &dest);
int  pointToPixel(int points);

// used in option migration
QString soundDetectPlayer();
void    soundPlay(const QString &);

extern Qt::WindowFlags psi_dialog_flags;

// like QT_VERSION, but runtime
int qVersionInt();

class Activity;
QString activityIconName(const Activity &);

QString macToQtDatetimeFormat(const QString &sys_fmt);

#endif // COMMON_H
