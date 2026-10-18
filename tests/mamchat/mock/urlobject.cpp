/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/urlobject.h"
#include <QApplication>
#include <QMenu>
URLObject::URLObject() : QObject(qApp), d(nullptr) { }
URLObject *URLObject::getInstance()
{
    static auto *object = new URLObject;
    return object;
}
QMenu *URLObject::createPopupMenu(const QString &) { return new QMenu; }
void   URLObject::popupAction(QString link) { emit openURL(link); }
