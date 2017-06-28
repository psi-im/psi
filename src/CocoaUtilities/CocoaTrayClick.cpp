/*
 * CocoaTrayClick
 * Copyright (C) 2012, 2017  Evgeny Khryukin
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
#include <objc/message.h>
#include <QApplication>

#include <QDebug>

//#define DEBUG_OUTPUT

bool dockClickHandler(id /*self*/, SEL /*_cmd*/, ...)
{
	CocoaTrayClick::instance()->emitTrayClicked();
	return true;
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
	Class cls = objc_getClass("NSApplication");
	objc_object *appInst = objc_msgSend((objc_object*)cls, sel_registerName("sharedApplication"));

	if(appInst != NULL) {
		objc_object* delegate = objc_msgSend(appInst, sel_registerName("delegate"));
		Class delClass = (Class)objc_msgSend(delegate,  sel_registerName("class"));
		SEL shouldHandle = sel_registerName("applicationShouldHandleReopen:hasVisibleWindows:");
		if (class_getInstanceMethod(delClass, shouldHandle)) {
			if (class_replaceMethod(delClass, shouldHandle, (IMP)dockClickHandler, "B@:")) {
#ifdef DEBUG_OUTPUT
				qDebug() << "Registered dock click handler (replaced original method)";
#endif
			}
			else {
				qWarning() << "Failed to replace method for dock click handler";
			}
		}
		else {
			if (class_addMethod(delClass, shouldHandle, (IMP)dockClickHandler,"B@:")) {
#ifdef DEBUG_OUTPUT
				qDebug() << "Registered dock click handler";
#endif
			}
			else {
				qWarning() << "Failed to register dock click handler";
			}
		}
	}
}

CocoaTrayClick::~CocoaTrayClick()
{
}

void CocoaTrayClick::emitTrayClicked()
{
	emit trayClicked();
}

CocoaTrayClick* CocoaTrayClick::instance_ = NULL;
