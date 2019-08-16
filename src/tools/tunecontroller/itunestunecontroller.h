#ifndef ITUNESTUNECONTROLLER_H
#define ITUNESTUNECONTROLLER_H

#include "tunecontrollerinterface.h"

#include <CoreFoundation/CoreFoundation.h>
#include <QString>

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

#endif // ITUNESTUNECONTROLLER_H
