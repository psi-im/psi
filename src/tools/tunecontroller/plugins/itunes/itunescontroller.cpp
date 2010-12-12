/*
 * itunescontroller.cpp 
 * Copyright (C) 2006  Remko Troncon
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

#include <QtGlobal>
#include <QString>
#include <QTime>
#include <QDebug>

#include <CoreFoundation/CoreFoundation.h>

#include "itunescontroller.h"

/**
 * \class ITunesController
 * \brief A controller for the Mac OS X version of iTunes.
 */

static QString CFStringToQString(CFStringRef s)
{
	QString result;

	if (s != NULL) {
		CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8) + 1;
		char* buffer = new char[length];
		if (CFStringGetCString(s, buffer, length, kCFStringEncodingUTF8)) {
			result = QString::fromUtf8(buffer);
		}
		else {
			qWarning("itunesplayer.cpp: CFString conversion failed.");
		}
		delete[] buffer;
	} 
    return result;
}


ITunesController::ITunesController()
{
	// TODO: Poll iTunes for current playing tune
	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
	CFNotificationCenterAddObserver(center, this, ITunesController::iTunesCallback, CFSTR("com.apple.iTunes.playerInfo"), NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
}

ITunesController::~ITunesController()
{
	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
	CFNotificationCenterRemoveObserver(center, this, CFSTR("com.apple.iTunes.playerInfo"), NULL);
}

Tune ITunesController::currentTune() 
{
	return currentTune_;
}

void ITunesController::iTunesCallback(CFNotificationCenterRef,void* observer,CFStringRef,const void*, CFDictionaryRef info)
{
	Tune tune;
	ITunesController* controller = (ITunesController*) observer;
	
	CFStringRef cf_state = (CFStringRef) CFDictionaryGetValue(info, CFSTR("Player State"));
	if (CFStringCompare(cf_state,CFSTR("Paused"),0) == kCFCompareEqualTo) {
		//qDebug() << "itunesplayer.cpp: Paused";
		emit controller->stopped();
	}
	else if (CFStringCompare(cf_state,CFSTR("Stopped"),0) == kCFCompareEqualTo) {
		//qDebug() << "itunesplayer.cpp: Stopped";
		emit controller->stopped();
	}
	else if (CFStringCompare(cf_state,CFSTR("Playing"),0) == kCFCompareEqualTo) {
		//qDebug() << "itunesplayer.cpp: Playing";
		tune.setArtist(CFStringToQString((CFStringRef) CFDictionaryGetValue(info, CFSTR("Artist"))));
		tune.setName(CFStringToQString((CFStringRef) CFDictionaryGetValue(info, CFSTR("Name"))));
		tune.setAlbum(CFStringToQString((CFStringRef) CFDictionaryGetValue(info, CFSTR("Album"))));
		
		CFNumberRef cf_track = (CFNumberRef) CFDictionaryGetValue(info, CFSTR("Track Number"));
		if (cf_track) {
			int tracknr;
			if (!CFNumberGetValue(cf_track,kCFNumberIntType,&tracknr)) {
				qWarning("itunesplayer.cpp: Number value conversion failed.");
			}
			tune.setTrack(QString::number(tracknr));
		}
		
		CFNumberRef cf_time = (CFNumberRef) CFDictionaryGetValue(info, CFSTR("Total Time"));
		int time = 0;
		if (cf_time && !CFNumberGetValue(cf_time,kCFNumberIntType,&time)) {
			qWarning("itunesplayer.cpp: Number value conversion failed.");
		}
		tune.setTime((unsigned int) (time / 1000));
		controller->currentTune_ = tune;
		emit controller->playing(tune);
	}
	else {
		qWarning("itunesplayer.cpp: Unknown state.");
	}
}
