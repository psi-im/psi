/*
 * idle.h - detect desktop idle time
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef IDLE_H
#define IDLE_H

#include <QObject>

class IdlePlatform;

class Idle : public QObject {
    Q_OBJECT
public:
    Idle();
    ~Idle();

    bool isActive() const;
    bool usingPlatform() const;
    void start();
    void stop();

signals:
    void secondsIdle(int);

private slots:
    void doCheck();

private:
    class Private;
    Private *d;
};

class IdlePlatform {
public:
    IdlePlatform();
    ~IdlePlatform();

    bool init();
    int  secondsIdle();

private:
    class Private;
    Private *d;
};

#endif // IDLE_H
