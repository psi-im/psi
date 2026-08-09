#ifndef MACDNCTUNECONTROLLER_H
#define MACDNCTUNECONTROLLER_H

#include "tunecontrollerinterface.h"

#include <CoreFoundation/CoreFoundation.h>
#include <QString>

class MacDNCController : public TuneController {
public:
    MacDNCController(CFStringRef notificationName);
    ~MacDNCController();

    virtual Tune currentTune() const;

private:
    static void NotificationCallback(CFNotificationCenterRef, void *, CFStringRef, const void *, CFDictionaryRef info);
    CFStringRef notificationName_;
    Tune        currentTune_;
};

#endif // MACDNCTUNECONTROLLER_H
