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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <QMap>
#include <QSize>
#include <QStringList>
#include <QList>
#include <QColor>

#include "statuspreset.h"

// -----------------------------------------------------------------------------
// Options
// -----------------------------------------------------------------------------

enum { dcClose, dcHour, dcDay, dcNever };


class ToolbarPrefs
{
public:
	ToolbarPrefs();

	QString id;
	QString name;
	QStringList keys;
	Qt::Dock dock;

	bool dirty;
	bool on;
	bool locked;
	// bool stretchable;

	// int index;
	bool nl;
	// int extraOffset;

	bool operator==(const ToolbarPrefs& other);
};

struct lateMigrationOptions
{

	QMap<QString, QString> serviceRosterIconset;
	QMap<QString, QString> customRosterIconset;

	QMap<QString,StatusPreset> sp; // Status message presets.

	QMap< QString, QList<ToolbarPrefs> > toolbars;
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

#include "xmpp_status.h"

#define STATUS_OFFLINE   XMPP::Status::Offline
#define STATUS_ONLINE    XMPP::Status::Online
#define STATUS_AWAY      XMPP::Status::Away
#define STATUS_XA        XMPP::Status::XA
#define STATUS_DND       XMPP::Status::DND
#define STATUS_INVISIBLE XMPP::Status::Invisible
#define STATUS_CHAT      XMPP::Status::FFC

#define STATUS_ASK	 100
#define STATUS_NOAUTH	 101
#define STATUS_ERROR	 102

QString status2txt(int status);
XMPP::Status makeStatus(int, const QString &);
XMPP::Status makeStatus(int, const QString &, int);
XMPP::Status::Type makeSTATUS(const XMPP::Status &);
QString clipStatus(const QString &str, int width, int height);


// -----------------------------------------------------------------------------
// Widget tools
// -----------------------------------------------------------------------------

void bringToFront(QWidget *w, bool grabFocus = true);
void replaceWidget(QWidget *, QWidget *);
void closeDialogs(QWidget *);
#ifdef Q_WS_X11
#include <QWidget>
void x11wmClass(Display *dsp, WId wid, QString resName);
#define X11WM_CLASS(x)	x11wmClass(x11Display(), winId(), (x));
#else
#define X11WM_CLASS(x)	/* dummy */
#endif

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

bool operator!=(const QMap<QString, QString> &, const QMap<QString, QString> &);

bool fileCopy(const QString &src, const QString &dest);


// used in option migration
QString soundDetectPlayer();
void soundPlay(const QString &);

extern Qt::WFlags psi_dialog_flags;

// like QT_VERSION, but runtime
int qVersionInt();

#endif
