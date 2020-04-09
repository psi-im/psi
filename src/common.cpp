/*
 * common.cpp - contains all the common variables and functions for Psi
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

#include "common.h"

#ifdef Q_OS_MAC
#include "CocoaUtilities/cocoacommon.h"
#endif
#include "activity.h"
#include "applicationinfo.h"
#include "profiles.h"
#include "psievent.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "rtparse.h"
#include "tabdlg.h"
#ifdef HAVE_X11
#include "x11windowsystem.h"
#endif

#include <QApplication>
#include <QBoxLayout>
#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QLibrary>
#include <QMenu>
#include <QMessageBox>
#include <QObject>
#include <QProcess>
#include <QRegExp>
#include <QSound>
#include <QUrl>
#include <QUuid>
#ifdef __GLIBC__
#include <langinfo.h>
#endif
#ifdef HAVE_KEYCHAIN
#include <qt5keychain/keychain.h>
#endif
#include <stdio.h>
#ifdef Q_OS_MAC
#include <Carbon/Carbon.h> // for HIToolbox/InternetConfig
#include <CoreServices/CoreServices.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

Qt::WindowFlags psi_dialog_flags = (Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint);

// used to be part of the global options struct.
// FIXME find it a new home!
int common_smallFontSize = 0;

QString CAP(const QString &str) { return QString("%1: %2").arg(ApplicationInfo::name()).arg(str); }

// clips plain text
QString clipStatus(const QString &str, int width, int height)
{
    QString out = "";
    int     at  = 0;
    int     len = str.length();
    if (len == 0)
        return out;

    // only take the first "height" lines
    for (int n2 = 0; n2 < height; ++n2) {
        // only take the first "width" chars
        QString line;
        bool    hasNewline = false;
        for (int n = 0; at < len; ++n, ++at) {
            if (str.at(at) == '\n') {
                hasNewline = true;
                break;
            }
            line += str.at(at);
        }
        ++at;
        if (int(line.length()) > width) {
            line.truncate(width - 3);
            line += "...";
        }
        out += line;
        if (hasNewline) {
            out += '\n';
        }

        if (at >= len) {
            break;
        }
    }

    return out;
}

QString encodePassword(const QString &pass, const QString &key)
{
    QString result;
    int     n1, n2;

    if (key.length() == 0) {
        return pass;
    }

    for (n1 = 0, n2 = 0; n1 < pass.length(); ++n1) {
        ushort  x = pass.at(n1).unicode() ^ key.at(n2++).unicode();
        QString hex;
        hex.sprintf("%04x", x);
        result += hex;
        if (n2 >= key.length()) {
            n2 = 0;
        }
    }
    return result;
}

QString decodePassword(const QString &pass, const QString &key)
{
    QString result;
    int     n1, n2;

    if (key.length() == 0) {
        return pass;
    }

    for (n1 = 0, n2 = 0; n1 < pass.length(); n1 += 4) {
        ushort x = 0;
        if (n1 + 4 > pass.length()) {
            break;
        }
        x += QString(pass.at(n1)).toInt(nullptr, 16) * 4096;
        x += QString(pass.at(n1 + 1)).toInt(nullptr, 16) * 256;
        x += QString(pass.at(n1 + 2)).toInt(nullptr, 16) * 16;
        x += QString(pass.at(n1 + 3)).toInt(nullptr, 16);
        QChar c(x ^ key.at(n2++).unicode());
        result += c;
        if (n2 >= key.length()) {
            n2 = 0;
        }
    }
    return result;
}

#ifdef HAVE_KEYCHAIN
void saveXMPPPasswordToKeyring(const QString &jid, const QString &pass, QObject *parent)
{
    auto pwJob = new QKeychain::WritePasswordJob(QLatin1String("xmpp"), parent);
    pwJob->setTextData(pass);
    pwJob->setKey(jid);
    pwJob->setAutoDelete(true);
    QObject::connect(pwJob, &QKeychain::Job::finished, parent, [](QKeychain::Job *job) {
        if (job->error() != QKeychain::NoError) {
            qWarning("Failed to save password in keyring manager");
        }
    });
    pwJob->start();
}

bool isKeychainEnabled()
{
    return !ApplicationInfo::isPortable()
        && PsiOptions::instance()->getOption("options.keychain.enabled", true).toBool();
}
#endif

QString status2txt(int status)
{
    switch (status) {
    case STATUS_OFFLINE:
        return QObject::tr("Offline");
    case STATUS_AWAY:
        return QObject::tr("Away");
    case STATUS_XA:
        return QObject::tr("Not Available");
    case STATUS_DND:
        return QObject::tr("Do not Disturb");
    case STATUS_CHAT:
        return QObject::tr("Free for Chat");
    case STATUS_INVISIBLE:
        return QObject::tr("Invisible");

    case STATUS_ONLINE:
    default:
        return QObject::tr("Online");
    }
}

QString logencode(QString str)
{
    str.replace(QRegExp("\\\\"), "\\\\"); // backslash to double-backslash
    str.replace(QRegExp("\\|"), "\\p");   // pipe to \p
    str.replace(QRegExp("\n"), "\\n");    // newline to \n
    return str;
}

QString logdecode(const QString &str)
{
    QString ret;

    for (int n = 0; n < str.length(); ++n) {
        if (str.at(n) == '\\') {
            ++n;
            if (n >= str.length()) {
                break;
            }
            if (str.at(n) == 'n') {
                ret.append('\n');
            }
            if (str.at(n) == 'p') {
                ret.append('|');
            }
            if (str.at(n) == '\\') {
                ret.append('\\');
            }
        } else {
            ret.append(str.at(n));
        }
    }

    return ret;
}

bool fileCopy(const QString &src, const QString &dest)
{
    QFile in(src);
    QFile out(dest);

    if (!in.open(QIODevice::ReadOnly)) {
        return false;
    }
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }

    char *dat = new char[16384];
    int   n   = 0;
    while (!in.atEnd()) {
        n = int(in.read(dat, 16384));
        if (n == -1) {
            delete[] dat;
            return false;
        }
        out.write(dat, n);
    }
    delete[] dat;

    out.close();
    in.close();

    return true;
}

/** Detect default player helper on unix like systems
 */
QString soundDetectPlayer()
{
    // prefer ALSA on linux
    if (QFile("/proc/asound").exists()) {
        return "aplay -q";
    }
    // fallback to "play"
    return "play";
}

void soundPlay(const QString &s)
{
    if (s.isEmpty())
        return;

    QString str = s;
    if (str == "!beep") {
        QApplication::beep();
        return;
    }

    if (QDir::isRelativePath(str)) {
        str = ApplicationInfo::resourcesDir() + '/' + str;
    }

    if (!QFile::exists(str)) {
        return;
    }

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    QSound::play(str);
#else
    QString player = PsiOptions::instance()->getOption("options.ui.notifications.sounds.unix-sound-player").toString();
    if (player == "")
        player = soundDetectPlayer();
    QStringList args = player.split(' ');
    args += str;
    QString prog = args.takeFirst();
    QProcess::startDetached(prog, args);
#endif
}

bool lastPriorityNotEmpty()
{
    QString lastPriority = PsiOptions::instance()->getOption("options.status.last-priority").toString();
    return (lastPriority != "");
}

XMPP::Status makeLastStatus(int x)
{
    PsiOptions *o = PsiOptions::instance();
    if (lastPriorityNotEmpty()) {
        return makeStatus(x, o->getOption("options.status.last-message").toString(),
                          o->getOption("options.status.last-priority").toInt());
    } else {
        return makeStatus(x, o->getOption("options.status.last-message").toString());
    }
}

XMPP::Status makeStatus(int x, const QString &str, int priority)
{
    XMPP::Status s = makeStatus(x, str);
    if (priority > 127) {
        s.setPriority(127);
    } else if (priority < -128) {
        s.setPriority(-128);
    } else {
        s.setPriority(priority);
    }
    return s;
}

XMPP::Status makeStatus(int x, const QString &str) { return XMPP::Status(static_cast<XMPP::Status::Type>(x), str); }

XMPP::Status::Type makeSTATUS(const XMPP::Status &s) { return s.type(); }

#include <QLayout>
QLayout *rw_recurseFindLayout(QLayout *lo, QWidget *w)
{
    // printf("scanning layout: %p\n", lo);
    for (int index = 0; index < lo->count(); ++index) {
        QLayoutItem *i = lo->itemAt(index);
        // printf("found: %p,%p\n", i->layout(), i->widget());
        QLayout *slo = i->layout();
        if (slo) {
            QLayout *tlo = rw_recurseFindLayout(slo, w);
            if (tlo) {
                return tlo;
            }
        } else if (i->widget() == w) {
            return lo;
        }
    }
    return nullptr;
}

QLayout *rw_findLayoutOf(QWidget *w) { return rw_recurseFindLayout(w->parentWidget()->layout(), w); }

void replaceWidget(QWidget *a, QWidget *b)
{
    if (!a) {
        return;
    }

    QLayout *lo = rw_findLayoutOf(a);
    if (!lo) {
        return;
    }
    // printf("decided on this: %p\n", lo);

    if (lo->inherits("QBoxLayout")) {
        QBoxLayout *bo = static_cast<QBoxLayout *>(lo);
        int         n  = bo->indexOf(a);
        bo->insertWidget(n + 1, b);
        delete a;
    }
}

void closeDialogs(QWidget *w)
{
    // close qmessagebox?
    QList<QDialog *> dialogs;
    QObjectList      list = w->children();
    for (QObjectList::Iterator it = list.begin(); it != list.end(); ++it) {
        if ((*it)->inherits("QDialog")) {
            dialogs.append(static_cast<QDialog *>(*it));
        }
    }
    foreach (QDialog *w, dialogs) {
        w->close();
    }
}

void reorderGridLayout(QGridLayout *layout, int maxCols)
{
    QList<QLayoutItem *> items;
    for (int i = 0; i < layout->rowCount(); i++) {
        for (int j = 0; j < layout->columnCount(); j++) {
            QLayoutItem *item = layout->itemAtPosition(i, j);
            if (item) {
                layout->removeItem(item);
                if (item->isEmpty()) {
                    delete item;
                } else {
                    items.append(item);
                }
            }
        }
    }
    int col = 0, row = 0;
    while (!items.isEmpty()) {
        QLayoutItem *item = items.takeAt(0);
        layout->addItem(item, row, col);
        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
}

TabbableWidget *findActiveTab()
{
    QWidget *       chat = QApplication::activeWindow();
    TabbableWidget *tw   = nullptr;
    if (chat) {
        TabDlg *td = qobject_cast<TabDlg *>(chat);
        if (td) {
            tw = td->getCurrentTab();
        } else {
            tw = qobject_cast<TabbableWidget *>(chat);
            if (!tw) {
                QList<TabDlg *> tmp = chat->findChildren<TabDlg *>(); // all-in-one
                while (!tmp.isEmpty()) {
                    TabDlg *td = tmp.takeFirst();
                    tw         = td->getCurrentTab();
                    if (tw) {
                        break;
                    }
                }
            }
        }
    }
    return tw;
}

void clearMenu(QMenu *m)
{
    m->clear();
    QObjectList l = m->children();
    foreach (QObject *obj, l) {
        QMenu *child = dynamic_cast<QMenu *>(obj);
        if (child) {
            delete child;
        }
    }
}

bool isKde()
{
    return qgetenv("XDG_SESSION_DESKTOP") == "KDE" || qgetenv("DESKTOP_SESSION").endsWith("plasma")
        || qgetenv("DESKTOP_SESSION").endsWith("plasma5");
}

void bringToFront(QWidget *widget, bool)
{
    Q_ASSERT(widget);
    QWidget *w = widget->window();

#ifdef HAVE_X11
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

    // FIXME: multi-desktop hacks for Win and Mac required
#endif

    if (w->isMaximized()) {
        w->showMaximized();
    } else {
        w->showNormal();
    }

    // if(grabFocus)
    //    w->setActiveWindow();
    w->raise();
    w->activateWindow();

#if 0
    // hack to real bring to front in kde. kde (at least 4.8.5) forbids stilling
    // focus from other applications. this may be fixed on more recent versions.
    // should be removed some day. preferable way for such hacks is plugins.
    // probably works only with gcc.
    //
    // with kde5 this code just crashes. so should be reimpented as a plugin
    if (isKde()) {
        typedef int (*ActWinFunction)(WId, long);
        ActWinFunction kwinActivateWindow = (ActWinFunction)QLibrary::resolve(
                    "libkdeui", 5, "_ZN13KWindowSystem17forceActiveWindowEml");
        if (kwinActivateWindow) {
            kwinActivateWindow(widget->winId(), 0);
        }
    }
#endif
}

bool operator!=(const QMap<QString, QString> &m1, const QMap<QString, QString> &m2)
{
    if (m1.size() != m2.size())
        return true;

    QMap<QString, QString>::ConstIterator it = m1.begin(), it2;
    for (; it != m1.end(); ++it) {
        it2 = m2.find(it.key());
        if (it2 == m2.end()) {
            return true;
        }
        if (it.value() != it2.value()) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------
// ToolbarPrefs
//----------------------------------------------------------------------------

ToolbarPrefs::ToolbarPrefs() :
    dock(Qt3Dock_Top)
    // , dirty(true)
    ,
    on(false), locked(false)
    // , stretchable(false)
    // , index(0)
    ,
    nl(true)
// , extraOffset(0)
{
    id = QUuid::createUuid().toString();
}

bool ToolbarPrefs::operator==(const ToolbarPrefs &other)
{
    return id == other.id && name == other.name && keys == other.keys && dock == other.dock &&
        // dirty == other.dirty &&
        on == other.on && locked == other.locked &&
        // stretchable == other.stretchable &&
        // index == other.index &&
        nl == other.nl;
    // extraOffset == other.extraOffset;
}

int versionStringToInt(const char *version)
{
    QString     str   = QString::fromLatin1(version);
    QStringList parts = str.split('.', QString::KeepEmptyParts);
    if (parts.count() != 3) {
        return 0;
    }

    int versionInt = 0;
    for (int n = 0; n < 3; ++n) {
        bool ok;
        int  x = parts[n].toInt(&ok);
        if (ok && x >= 0 && x <= 0xff) {
            versionInt <<= 8;
            versionInt += x;
        } else {
            return 0;
        }
    }
    return versionInt;
}

int qVersionInt()
{
    static int out = -1;
    if (out == -1) {
        out = versionStringToInt(qVersion());
    }
    return out;
}

QString activityIconName(const Activity &activity)
{
    if (activity.type() == Activity::Unknown) {
        return QString();
    }
    if (activity.specificType() == Activity::Other) {
        return QLatin1String("pep/activities");
    }
    if (activity.specificType() == Activity::UnknownSpecific || activity.specificTypeValue().isEmpty()) {
        return QLatin1String("activities/") + activity.typeValue();
    }
    return QLatin1String("activities/") + activity.typeValue() + QLatin1Char('_') + activity.specificTypeValue();
}

#ifdef WEBKIT
// all these functions in ifdef were copied from
// https://raw.githubusercontent.com/qt/qtbase/dev/src/corelib/tools/{qlocale.cpp,qlocale_mac.mm} // 25 Dec 2016
// and required to support proper date formatting in Adium themes
static QString qt_readEscapedFormatString(const QString &format, int *idx)
{
    int &i = *idx;

    Q_ASSERT(format.at(i) == QLatin1Char('\''));
    ++i;
    if (i == format.size())
        return QString();
    if (format.at(i).unicode() == '\'') { // "''" outside of a quoted stirng
        ++i;
        return QLatin1String("'");
    }

    QString result;

    while (i < format.size()) {
        if (format.at(i).unicode() == '\'') {
            if (i + 1 < format.size() && format.at(i + 1).unicode() == '\'') {
                // "''" inside of a quoted string
                result.append(QLatin1Char('\''));
                i += 2;
            } else {
                break;
            }
        } else {
            result.append(format.at(i++));
        }
    }
    if (i < format.size())
        ++i;

    return result;
}

static int qt_repeatCount(const QString &s, int i)
{
    QChar c = s.at(i);
    int   j = i + 1;
    while (j < s.size() && s.at(j) == c)
        ++j;
    return j - i;
}

// original name macToQtFormat
QString macToQtDatetimeFormat(const QString &sys_fmt)
{
    QString result;
    int     i = 0;

    while (i < sys_fmt.size()) {
        if (sys_fmt.at(i).unicode() == '\'') {
            QString text = qt_readEscapedFormatString(sys_fmt, &i);
            if (text == QLatin1String("'"))
                result += QLatin1String("''");
            else
                result += QLatin1Char('\'') + text + QLatin1Char('\'');
            continue;
        }

        QChar c      = sys_fmt.at(i);
        int   repeat = qt_repeatCount(sys_fmt, i);

        switch (c.unicode()) {
        // Qt does not support the following options
        case 'G': // Era (1..5): 4 = long, 1..3 = short, 5 = narrow
        case 'Y': // Year of Week (1..n): 1..n = padded number
        case 'U': // Cyclic Year Name (1..5): 4 = long, 1..3 = short, 5 = narrow
        case 'Q': // Quarter (1..4): 4 = long, 3 = short, 1..2 = padded number
        case 'q': // Standalone Quarter (1..4): 4 = long, 3 = short, 1..2 = padded number
        case 'w': // Week of Year (1..2): 1..2 = padded number
        case 'W': // Week of Month (1): 1 = number
        case 'D': // Day of Year (1..3): 1..3 = padded number
        case 'F': // Day of Week in Month (1): 1 = number
        case 'g': // Modified Julian Day (1..n): 1..n = padded number
        case 'A': // Milliseconds in Day (1..n): 1..n = padded number
            break;

        case 'y': // Year (1..n): 2 = short year, 1 & 3..n = padded number
        case 'u': // Extended Year (1..n): 2 = short year, 1 & 3..n = padded number
            // Qt only supports long (4) or short (2) year, use long for all others
            if (repeat == 2)
                result += QLatin1String("yy");
            else
                result += QLatin1String("yyyy");
            break;
        case 'M': // Month (1..5): 4 = long, 3 = short, 1..2 = number, 5 = narrow
        case 'L': // Standalone Month (1..5): 4 = long, 3 = short, 1..2 = number, 5 = narrow
            // Qt only supports long, short and number, use short for narrow
            if (repeat == 5)
                result += QLatin1String("MMM");
            else
                result += QString(repeat, QLatin1Char('M'));
            break;
        case 'd': // Day of Month (1..2): 1..2 padded number
            result += QString(repeat, c);
            break;
        case 'E': // Day of Week (1..6): 4 = long, 1..3 = short, 5..6 = narrow
            // Qt only supports long, short and padded number, use short for narrow
            if (repeat == 4)
                result += QLatin1String("dddd");
            else
                result += QLatin1String("ddd");
            break;
        case 'e': // Local Day of Week (1..6): 4 = long, 3 = short, 5..6 = narrow, 1..2 padded number
        case 'c': // Standalone Local Day of Week (1..6): 4 = long, 3 = short, 5..6 = narrow, 1..2 padded number
            // Qt only supports long, short and padded number, use short for narrow
            if (repeat >= 5)
                result += QLatin1String("ddd");
            else
                result += QString(repeat, QLatin1Char('d'));
            break;
        case 'a': // AM/PM (1): 1 = short
            // Translate to Qt uppercase AM/PM
            result += QLatin1String("AP");
            break;
        case 'h': // Hour [1..12] (1..2): 1..2 = padded number
        case 'K': // Hour [0..11] (1..2): 1..2 = padded number
        case 'j': // Local Hour [12 or 24] (1..2): 1..2 = padded number
            // Qt h is local hour
            result += QString(repeat, QLatin1Char('h'));
            break;
        case 'H': // Hour [0..23] (1..2): 1..2 = padded number
        case 'k': // Hour [1..24] (1..2): 1..2 = padded number
            // Qt H is 0..23 hour
            result += QString(repeat, QLatin1Char('H'));
            break;
        case 'm': // Minutes (1..2): 1..2 = padded number
        case 's': // Seconds (1..2): 1..2 = padded number
            result += QString(repeat, c);
            break;
        case 'S': // Fractional second (1..n): 1..n = truncates to decimal places
            // Qt uses msecs either unpadded or padded to 3 places
            if (repeat < 3)
                result += QLatin1Char('z');
            else
                result += QLatin1String("zzz");
            break;
        case 'z': // Time Zone (1..4)
        case 'Z': // Time Zone (1..5)
        case 'O': // Time Zone (1, 4)
        case 'v': // Time Zone (1, 4)
        case 'V': // Time Zone (1..4)
        case 'X': // Time Zone (1..5)
        case 'x': // Time Zone (1..5)
            result += QLatin1Char('t');
            break;
        default:
            // a..z and A..Z are reserved for format codes, so any occurrence of these not
            // already processed are not known and so unsupported formats to be ignored.
            // All other chars are allowed as literals.
            if (c < QLatin1Char('A') || c > QLatin1Char('z') || (c > QLatin1Char('Z') && c < QLatin1Char('a'))) {
                result += QString(repeat, c);
            }
            break;
        }

        i += repeat;
    }

    return result;
}
#endif

int pointToPixel(int points)
{
    // In typography 1 point (also called PostScript point)
    // is 1/72 of an inch
    static const double postScriptPoint = 1 / 72.;

    return qRound(points * (qApp->desktop()->logicalDpiX() * postScriptPoint));
}
