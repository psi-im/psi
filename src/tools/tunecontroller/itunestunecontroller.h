#ifndef ITUNESTUNECONTROLLER_H
#define ITUNESTUNECONTROLLER_H

#include <QString>
#include <CoreFoundation/CoreFoundation.h>

#include "tunecontrollerinterface.h"

class ITunesController : public TuneController
{
public:
	ITunesController();
	~ITunesController();

	virtual Tune currentTune() const;

private:
	static void iTunesCallback(CFNotificationCenterRef,void*,CFStringRef,const void*, CFDictionaryRef info);
	Tune currentTune_;
};

#endif
