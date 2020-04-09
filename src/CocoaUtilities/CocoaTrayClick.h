/*
 * CocoaTrayClick
 * Copyright (C) 2012  Evgeny Khryukin
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

#ifndef COCOATRAYCLICK_H
#define COCOATRAYCLICK_H

#include <QObject>

class CocoaTrayClick : public QObject {
    Q_OBJECT
public:
    static CocoaTrayClick *instance();
    ~CocoaTrayClick();

    void emitTrayClicked();

signals:
    void trayClicked();

private:
    CocoaTrayClick();
    static CocoaTrayClick *instance_;
};

#endif // COCOATRAYCLICK_H
