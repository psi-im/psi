/*
 * globalshortcutmanager.h - Class managing global shortcuts
 * Copyright (C) 2006  Maciej Niedzielski
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

#ifndef GLOBALSHORTCUTMANAGER_H
#define GLOBALSHORTCUTMANAGER_H

#include <QKeySequence>
#include <QMap>
#include <QObject>

class KeyTrigger;
class QObject;

class GlobalShortcutManager : public QObject {
public:
    static GlobalShortcutManager *instance();
    static void                   connect(const QKeySequence &key, QObject *receiver, const char *slot);
    static void                   disconnect(const QKeySequence &key, QObject *receiver, const char *slot);
    static void                   clear();

private:
    GlobalShortcutManager();
    ~GlobalShortcutManager();
    static GlobalShortcutManager *instance_;
    class KeyTrigger;
    QMap<QKeySequence, KeyTrigger *> triggers_;
};

#endif // GLOBALSHORTCUTMANAGER_H
