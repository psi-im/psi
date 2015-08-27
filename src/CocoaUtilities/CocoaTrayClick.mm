/*
 * CocoaTrayClick
 * Copyright (C) 2012  Khryukin Evgeny
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "CocoaUtilities/CocoaTrayClick.h"
#include <objc/runtime.h>
#include <QApplication>
#include <QDebug>
#ifdef HAVE_QT5
#include <AppKit/NSApplication.h>
#endif

void dockClickHandler(id /*self*/, SEL /*_cmd*/)
{
	CocoaTrayClick::instance()->emitTrayClicked();
}


CocoaTrayClick * CocoaTrayClick::instance()
{
	if(!instance_)
		instance_ = new CocoaTrayClick();

	return instance_;
}

CocoaTrayClick::CocoaTrayClick()
	: QObject(qApp)
{
	Class cls = [[[NSApplication sharedApplication] delegate] class];
	if (!class_addMethod(cls, @selector(applicationShouldHandleReopen:hasVisibleWindows:), (IMP) dockClickHandler, "v@:"))
		qDebug() << "CocoaTrayClick::Private : class_addMethod failed!";
}

CocoaTrayClick::~CocoaTrayClick()
{
}

void CocoaTrayClick::emitTrayClicked()
{
	emit trayClicked();
}

CocoaTrayClick* CocoaTrayClick::instance_ = NULL;
