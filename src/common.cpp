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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "common.h"
#include "profiles.h"
#include "rtparse.h"
#include "psievent.h"
#include "psiiconset.h"
#include "applicationinfo.h"
#include "psioptions.h"

#include <QUrl>
#include <QProcess>
#include <QBoxLayout>
#include <QRegExp>
#include <QFile>
#include <QApplication>
#include <QSound>
#include <QObject>
#include <QMessageBox>
#include <QUuid>
#include <QDir>

#include <stdio.h>
#ifdef Q_WS_X11
#include <QX11Info>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

#ifdef Q_WS_WIN
#include <windows.h>
#endif

#ifdef Q_WS_MAC
#include <sys/types.h>
#include <sys/stat.h>
#include <Carbon/Carbon.h> // for HIToolbox/InternetConfig
#include <CoreServices/CoreServices.h>
#include "CocoaUtilities/cocoacommon.h"
#endif

#ifdef __GLIBC__
#include <langinfo.h>
#endif

Qt::WFlags psi_dialog_flags = (Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint);

// used to be part of the global options struct. 
// FIXME find it a new home!
int common_smallFontSize=0;


bool useSound;


QString CAP(const QString &str)
{
	return QString("%1: %2").arg(ApplicationInfo::name()).arg(str);
}


// clips plain text
QString clipStatus(const QString &str, int width, int height)
{
	QString out = "";
	int at = 0;
	int len = str.length();
	if(len == 0)
		return out;

	// only take the first "height" lines
	for(int n2 = 0; n2 < height; ++n2) {
		// only take the first "width" chars
		QString line;
		bool hasNewline = false;
		for(int n = 0; at < len; ++n, ++at) {
			if(str.at(at) == '\n') {
				hasNewline = true;
				break;
			}
			line += str.at(at);
		}
		++at;
		if((int)line.length() > width) {
			line.truncate(width-3);
			line += "...";
		}
		out += line;
		if(hasNewline) {
			out += '\n';
		}

		if(at >= len) {
			break;
		}
	}

	return out;
}

QString encodePassword(const QString &pass, const QString &key)
{
	QString result;
	int n1, n2;

	if (key.length() == 0) {
		return pass;
	}

	for (n1 = 0, n2 = 0; n1 < pass.length(); ++n1) {
		ushort x = pass.at(n1).unicode() ^ key.at(n2++).unicode();
		QString hex;
		hex.sprintf("%04x", x);
		result += hex;
		if(n2 >= key.length()) {
			n2 = 0;
		}
	}
	return result;
}

QString decodePassword(const QString &pass, const QString &key)
{
	QString result;
	int n1, n2;

	if (key.length() == 0) {
		return pass;
	}

	for(n1 = 0, n2 = 0; n1 < pass.length(); n1 += 4) {
		ushort x = 0;
		if(n1 + 4 > pass.length()) {
			break;
		}
		x += QString(pass.at(n1)).toInt(NULL,16)*4096;
		x += QString(pass.at(n1+1)).toInt(NULL,16)*256;
		x += QString(pass.at(n1+2)).toInt(NULL,16)*16;
		x += QString(pass.at(n1+3)).toInt(NULL,16);
		QChar c(x ^ key.at(n2++).unicode());
		result += c;
		if(n2 >= key.length()) {
			n2 = 0;
		}
	}
	return result;
}

QString status2txt(int status)
{
	switch(status) {
		case STATUS_OFFLINE:	return QObject::tr("Offline");
		case STATUS_AWAY:		return QObject::tr("Away");
		case STATUS_XA:			return QObject::tr("Not Available");
		case STATUS_DND:		return QObject::tr("Do not Disturb");
		case STATUS_CHAT:		return QObject::tr("Free for Chat");
		case STATUS_INVISIBLE:	return QObject::tr("Invisible");

		case STATUS_ONLINE:
		default:				return QObject::tr("Online");
	}
}


QString logencode(QString str)
{
	str.replace(QRegExp("\\\\"), "\\\\");   // backslash to double-backslash
	str.replace(QRegExp("\\|"), "\\p");	 // pipe to \p
	str.replace(QRegExp("\n"), "\\n");	  // newline to \n
	return str;
}

QString logdecode(const QString &str)
{
	QString ret;

	for(int n = 0; n < str.length(); ++n) {
		if(str.at(n) == '\\') {
			++n;
			if(n >= str.length()) {
				break;
			}
			if(str.at(n) == 'n') {
				ret.append('\n');
			}
			if(str.at(n) == 'p') {
				ret.append('|');
			}
			if(str.at(n) == '\\') {
				ret.append('\\');
			}
		}
		else {
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
	int n = 0;
	while (!in.atEnd()) {
		n = in.read(dat, 16384);
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
		return "aplay";
	}
	// fallback to "play"
	return "play";
	
}

void soundPlay(const QString &s)
{
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

#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
	QSound::play(str);
#else
	QString player = PsiOptions::instance()->getOption("options.ui.notifications.sounds.unix-sound-player").toString();
	if (player == "") player = soundDetectPlayer();
	QStringList args = player.split(' ');
	args += str;
	QString prog = args.takeFirst();
	QProcess::startDetached(prog, args);
#endif
}

XMPP::Status makeStatus(int x, const QString &str, int priority)
{
	XMPP::Status s = makeStatus(x,str);
	if (priority > 127) {
		s.setPriority(127);
	}
	else if (priority < -128) {
		s.setPriority(-128);
	}
	else {
		s.setPriority(priority);
	}
	return s;
}

XMPP::Status makeStatus(int x, const QString &str)
{
	return XMPP::Status(static_cast<XMPP::Status::Type>(x), str);
}

XMPP::Status::Type makeSTATUS(const XMPP::Status &s)
{
	return s.type();
}

#include <QLayout>
QLayout *rw_recurseFindLayout(QLayout *lo, QWidget *w)
{
	//printf("scanning layout: %p\n", lo);
	for (int index = 0; index < lo->count(); ++index) {
		QLayoutItem* i = lo->itemAt(index);
		//printf("found: %p,%p\n", i->layout(), i->widget());
		QLayout *slo = i->layout();
		if(slo) {
			QLayout *tlo = rw_recurseFindLayout(slo, w);
			if(tlo) {
				return tlo;
			}
		}
		else if(i->widget() == w) {
			return lo;
		}
	}
	return 0;
}

QLayout *rw_findLayoutOf(QWidget *w)
{
	return rw_recurseFindLayout(w->parentWidget()->layout(), w);
}

void replaceWidget(QWidget *a, QWidget *b)
{
	if(!a) {
		return;
	}

	QLayout *lo = rw_findLayoutOf(a);
	if(!lo) {
		return;
	}
	//printf("decided on this: %p\n", lo);

	if(lo->inherits("QBoxLayout")) {
		QBoxLayout *bo = (QBoxLayout *)lo;
		int n = bo->indexOf(a);
		bo->insertWidget(n+1, b);
		delete a;
	}
}

void closeDialogs(QWidget *w)
{
	// close qmessagebox?
	QList<QDialog*> dialogs;
	QObjectList list = w->children();
	for (QObjectList::Iterator it = list.begin() ; it != list.end(); ++it) {
		if((*it)->inherits("QDialog")) {
			dialogs.append((QDialog *)(*it));
		}
	}
	foreach (QDialog *w, dialogs) {
		w->close();
	}
}

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h> // needed for WM_CLASS hinting

void x11wmClass(Display *dsp, WId wid, QString resName)
{
	char app_name[] = "psi";

	//Display *dsp = x11Display();				 // get the display
	//WId win = winId();						   // get the window
	XClassHint classhint;						  // class hints
	const QByteArray latinResName = resName.toLatin1();
	classhint.res_name = (char *)latinResName.data(); // res_name
	classhint.res_class = app_name;				// res_class
	XSetClassHint(dsp, wid, &classhint);		   // set the class hints
}

//>>>-- Nathaniel Gray -- Caltech Computer Science ------>
//>>>-- Mojave Project -- http://mojave.cs.caltech.edu -->
// Copied from http://www.nedit.org/archives/discuss/2002-Aug/0386.html

// Helper function
bool getCardinal32Prop(Display *display, Window win, char *propName, long *value)
{
	Atom nameAtom, typeAtom, actual_type_return;
	int actual_format_return, result;
	unsigned long nitems_return, bytes_after_return;
	long *result_array=NULL;

	nameAtom = XInternAtom(display, propName, False);
	typeAtom = XInternAtom(display, "CARDINAL", False);
	if (nameAtom == None || typeAtom == None) {
		//qDebug("Atoms not interned!");
		return false;
	}


	// Try to get the property
	result = XGetWindowProperty(display, win, nameAtom, 0, 1, False,
		typeAtom, &actual_type_return, &actual_format_return,
		&nitems_return, &bytes_after_return,
		(unsigned char **)&result_array);

	if( result != Success ) {
		//qDebug("not Success");
		return false;
	}
	if( actual_type_return == None || actual_format_return == 0 ) {
		//qDebug("Prop not found");
		return false;
	}
	if( actual_type_return != typeAtom ) {
		//qDebug("Wrong type atom");
	}
	*value = result_array[0];
	XFree(result_array);
	return true;
}


// Get the desktop number that a window is on
bool desktopOfWindow(Window *window, long *desktop)
{
	Display *display = QX11Info::display();
	bool result = getCardinal32Prop(display, *window, (char *)"_NET_WM_DESKTOP", desktop);
	//if( result )
	//	qDebug("Desktop: " + QString::number(*desktop));
	return result;
}


// Get the current desktop the WM is displaying
bool currentDesktop(long *desktop)
{
	Window rootWin;
	Display *display = QX11Info::display();
	bool result;

	rootWin = RootWindow(QX11Info::display(), XDefaultScreen(QX11Info::display()));
	result = getCardinal32Prop(display, rootWin, (char *)"_NET_CURRENT_DESKTOP", desktop);
	//if( result )
	//	qDebug("Current Desktop: " + QString::number(*desktop));
	return result;
}
#endif

void bringToFront(QWidget *widget, bool)
{
	Q_ASSERT(widget);
	QWidget* w = widget->window();

#ifdef Q_WS_X11
	// If we're not on the current desktop, do the hide/show trick
	long dsk, curr_dsk;
	Window win = w->winId();
	if(desktopOfWindow(&win, &dsk) && currentDesktop(&curr_dsk)) {
		//qDebug() << "bringToFront current desktop=" << curr_dsk << " windowDesktop=" << dsk;
		if((dsk != curr_dsk) && (dsk != -1)) {  // second condition for sticky windows
			w->hide();
		}
	}

	// FIXME: multi-desktop hacks for Win and Mac required
#endif

	if(w->isMaximized()) {
		w->showMaximized();
	}
	else {
		w->showNormal();
	}

	//if(grabFocus)
	//	w->setActiveWindow();
	w->raise();
	w->activateWindow();
}

bool operator!=(const QMap<QString, QString> &m1, const QMap<QString, QString> &m2)
{
	if ( m1.size() != m2.size() )
		return true;

	QMap<QString, QString>::ConstIterator it = m1.begin(), it2;
	for ( ; it != m1.end(); ++it) {
		it2 = m2.find( it.key() );
		if ( it2 == m2.end() ) {
			return true;
		}
		if ( it.value() != it2.value() ) {
			return true;
		}
	}

	return false;
}

//----------------------------------------------------------------------------
// ToolbarPrefs
//----------------------------------------------------------------------------

ToolbarPrefs::ToolbarPrefs()
	: dock(Qt3Dock_Top)
	// , dirty(true)
	, on(false)
	, locked(false)
	// , stretchable(false)
	// , index(0)
	, nl(true)
	// , extraOffset(0)
{
	id = QUuid::createUuid().toString();
}

bool ToolbarPrefs::operator==(const ToolbarPrefs& other)
{
	return id == other.id &&
		   name == other.name &&
		   keys == other.keys &&
		   dock == other.dock &&
		   // dirty == other.dirty &&
		   on == other.on &&
		   locked == other.locked &&
		   // stretchable == other.stretchable &&
		   // index == other.index &&
		   nl == other.nl;
		   // extraOffset == other.extraOffset;
}


int versionStringToInt(const char* version)
{
	QString str = QString::fromLatin1(version);
	QStringList parts = str.split('.', QString::KeepEmptyParts);
	if (parts.count() != 3) {
		return 0;
	}

	int versionInt = 0;
	for (int n = 0; n < 3; ++n) {
		bool ok;
		int x = parts[n].toInt(&ok);
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

Qt::DayOfWeek firstDayOfWeekFromLocale()
{
	Qt::DayOfWeek firstDay = Qt::Monday;
#ifdef Q_OS_WIN
	WCHAR wsDay[4];
# if WINVER >= _WIN32_WINNT_VISTA && defined(LOCALE_NAME_USER_DEFAULT)
	if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_IFIRSTDAYOFWEEK, wsDay, 4)) {
# else
	if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IFIRSTDAYOFWEEK, wsDay, 4)) {
# endif
		bool ok;
		int wfd = QString::fromWCharArray(wsDay).toInt(&ok) + 1;
		if (ok) {
			firstDay = (Qt::DayOfWeek)(unsigned char)wfd;
		}
	}
#elif defined(__GLIBC__)
	firstDay = (Qt::DayOfWeek)(unsigned char)((*nl_langinfo(_NL_TIME_FIRST_WEEKDAY) + 5) % 7 + 1);
#elif defined(Q_OS_MAC)
	firstDay = (Qt::DayOfWeek)(unsigned char)macosCommonFirstWeekday();
#endif
	return firstDay;
}
