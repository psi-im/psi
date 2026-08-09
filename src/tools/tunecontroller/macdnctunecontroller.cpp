/*
 * macdnctunecontroller.cpp
 * Copyright (C) 2006  Remko Troncon
 * Copyright (C) 2026  Taylor Fox
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

#include "macdnctunecontroller.h"

#include <CoreFoundation/CoreFoundation.h>
#include <QDebug>
#include <QString>
#include <QTime>
#include <QtGlobal>

/**
 * \class MacDNCController
 * \brief A controller for the Mac OS X version of iTunes.
 */

static QString CFStringToQString(CFStringRef s)
{
    QString result;

    if (s != NULL) {
        CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8) + 1;
        char   *buffer = new char[length];
        if (CFStringGetCString(s, buffer, length, kCFStringEncodingUTF8)) {
            result = QString::fromUtf8(buffer);
        } else {
            qWarning("macdnctunecontroller.cpp: CFString conversion failed.");
        }
        delete[] buffer;
    }
    return result;
}

MacDNCController::MacDNCController(CFStringRef notificationName) : notificationName_(notificationName)
{
    CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
    CFNotificationCenterAddObserver(center, this, MacDNCController::NotificationCallback,
                                    notificationName_, NULL,
                                    CFNotificationSuspensionBehaviorDeliverImmediately);
}

MacDNCController::~MacDNCController()
{
    CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
    CFNotificationCenterRemoveObserver(center, this, notificationName_, NULL);
}

Tune MacDNCController::currentTune() const { return currentTune_; }

void MacDNCController::NotificationCallback(CFNotificationCenterRef, void *observer, CFStringRef, const void *,
                                      CFDictionaryRef info)
{
    Tune              tune;
    MacDNCController *controller = (MacDNCController *)observer;

    CFStringRef cf_state = (CFStringRef)CFDictionaryGetValue(info, CFSTR("Player State"));
    if (CFStringCompare(cf_state, CFSTR("Paused"), 0) == kCFCompareEqualTo) {
        // qDebug() << "macdnctunecontroller.cpp: Paused";
        emit controller->stopped();
    } else if (CFStringCompare(cf_state, CFSTR("Stopped"), 0) == kCFCompareEqualTo) {
        // qDebug() << "macdnctunecontroller.cpp: Stopped";
        emit controller->stopped();
    } else if (CFStringCompare(cf_state, CFSTR("Playing"), 0) == kCFCompareEqualTo) {
        // qDebug() << "macdnctunecontroller.cpp: Playing";
        tune.setArtist(CFStringToQString((CFStringRef)CFDictionaryGetValue(info, CFSTR("Artist"))));
        tune.setName(CFStringToQString((CFStringRef)CFDictionaryGetValue(info, CFSTR("Name"))));
        tune.setAlbum(CFStringToQString((CFStringRef)CFDictionaryGetValue(info, CFSTR("Album"))));

        CFNumberRef cf_track = (CFNumberRef)CFDictionaryGetValue(info, CFSTR("Track Number"));
        if (cf_track) {
            int tracknr;
            if (!CFNumberGetValue(cf_track, kCFNumberIntType, &tracknr)) {
                qWarning("macdnctunecontroller.cpp: Number value conversion failed.");
            }
            tune.setTrack(QString::number(tracknr));
        }

        CFNumberRef cf_time = (CFNumberRef)CFDictionaryGetValue(info, CFSTR("Total Time"));
        if (!cf_time) cf_time = (CFNumberRef)CFDictionaryGetValue(info, CFSTR("Duration")); // Spotify uses 'Duration' instead of 'Total Time'
        int         time    = 0;
        if (cf_time && !CFNumberGetValue(cf_time, kCFNumberIntType, &time)) {
            qWarning("macdnctunecontroller.cpp: Number value conversion failed.");
        }
        tune.setTime((unsigned int)(time / 1000));

        CFStringRef cf_trackId = (CFStringRef)CFDictionaryGetValue(info, CFSTR("Track ID"));
        if (cf_trackId && CFStringHasPrefix(cf_trackId, CFSTR("spotify:"))) {
            QString url = CFStringToQString(cf_trackId)
                .replace(QChar(':'), QChar('/'))
                .replace("spotify/", "https://open.spotify.com/");
            tune.setURL(url);
        }

        controller->currentTune_ = tune;
        emit controller->playing(tune);
    } else {
        qWarning("macdnctunecontroller.cpp: Unknown state.");
    }
}
