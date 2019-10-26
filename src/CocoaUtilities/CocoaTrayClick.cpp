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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "CocoaUtilities/CocoaTrayClick.h"

#include <QApplication>
#include <QDebug>
#include <objc/message.h>
#include <objc/runtime.h>

//#define DEBUG_OUTPUT

typedef objc_object* (*object_type)(struct objc_object *self, SEL _cmd);
object_type objc_msgSendObject = (object_type)objc_msgSend;

bool dockClickHandler(id /*self*/, SEL /*_cmd*/, ...)
{
    CocoaTrayClick::instance()->emitTrayClicked();
    return true;
}

CocoaTrayClick *CocoaTrayClick::instance()
{
    if (!instance_)
        instance_ = new CocoaTrayClick();

    return instance_;
}

CocoaTrayClick::CocoaTrayClick() : QObject(qApp)
{
    Class        cls     = objc_getClass("NSApplication");
    objc_object *appInst = objc_msgSendObject((objc_object *)cls, sel_registerName("sharedApplication"));

    if (appInst != NULL) {
        objc_object *delegate     = objc_msgSendObject(appInst, sel_registerName("delegate"));
        Class        delClass     = (Class)objc_msgSendObject(delegate, sel_registerName("class"));
        SEL          shouldHandle = sel_registerName("applicationShouldHandleReopen:hasVisibleWindows:");
        if (class_getInstanceMethod(delClass, shouldHandle)) {
            if (class_replaceMethod(delClass, shouldHandle, (IMP)dockClickHandler, "B@:")) {
#ifdef DEBUG_OUTPUT
                qDebug() << "Registered dock click handler (replaced original method)";
#endif
            } else {
                qWarning() << "Failed to replace method for dock click handler";
            }
        } else {
            if (class_addMethod(delClass, shouldHandle, (IMP)dockClickHandler, "B@:")) {
#ifdef DEBUG_OUTPUT
                qDebug() << "Registered dock click handler";
#endif
            } else {
                qWarning() << "Failed to register dock click handler";
            }
        }
    }
}

CocoaTrayClick::~CocoaTrayClick() {}

void CocoaTrayClick::emitTrayClicked() { emit trayClicked(); }

CocoaTrayClick *CocoaTrayClick::instance_ = NULL;
