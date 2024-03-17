/*
 * idle.cpp - detect desktop idle time
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

#include "idle.h"

#include <QCursor>
#include <QDateTime>
#include <QTimer>

static IdlePlatform *platform     = nullptr;
static int           platform_ref = 0;

class Idle::Private {
public:
    Private() = default;

    QPoint    lastMousePos;
    QDateTime idleSince;

    bool      active   = false;
    int       idleTime = 0;
    QDateTime startTime;
    QTimer    checkTimer;
};

Idle::Idle()
{
    d = new Private;

    // try to use platform idle
    if (!platform) {
        IdlePlatform *p = new IdlePlatform;
        if (p->init())
            platform = p;
        else
            delete p;
    }
    if (platform)
        ++platform_ref;

    connect(&d->checkTimer, SIGNAL(timeout()), SLOT(doCheck()));
}

Idle::~Idle()
{
    if (platform) {
        --platform_ref;
        if (platform_ref == 0) {
            delete platform;
            platform = nullptr;
        }
    }
    delete d;
}

bool Idle::isActive() const { return d->active; }

bool Idle::usingPlatform() const { return (platform ? true : false); }

void Idle::start()
{
    d->startTime = QDateTime::currentDateTime();

    if (!platform) {
        // generic idle
        d->lastMousePos = QCursor::pos();
        d->idleSince    = QDateTime::currentDateTime();
    }

    // poll every 5 seconds (use a lower value if you need more accuracy)
    d->checkTimer.start(5000);
}

void Idle::stop() { d->checkTimer.stop(); }

void Idle::doCheck()
{
    int i;
    if (platform)
        i = platform->secondsIdle();
    else {
        QPoint    curMousePos = QCursor::pos();
        QDateTime curDateTime = QDateTime::currentDateTime();
        if (d->lastMousePos != curMousePos) {
            d->lastMousePos = curMousePos;
            d->idleSince    = curDateTime;
        }
        i = d->idleSince.secsTo(curDateTime);
    }

    // set 'beginIdle' to the beginning of the idle time (by backtracking 'i' seconds from now)
    QDateTime beginIdle = QDateTime::currentDateTime().addSecs(-i);

    // set 't' to hold the number of seconds between 'beginIdle' and 'startTime'
    int t = beginIdle.secsTo(d->startTime);

    // beginIdle later than (or equal to) startTime?
    if (t <= 0) {
        // scoot ourselves up to the new idle start
        d->startTime = beginIdle;
    }
    // beginIdle earlier than startTime?
    else if (t > 0) {
        // do nothing
    }

    // how long have we been idle?
    int idleTime = d->startTime.secsTo(QDateTime::currentDateTime());

    emit secondsIdle(idleTime);
}
