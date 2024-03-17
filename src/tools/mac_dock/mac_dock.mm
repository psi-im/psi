#include "mac_dock.h"

#include "privateqt_mac.h"

#include <Cocoa/Cocoa.h>

static NSInteger requestType = 0;

void MacDock::startBounce() { requestType = [NSApp requestUserAttention:NSCriticalRequest]; }

void MacDock::stopBounce()
{
    if (requestType) {
        [NSApp cancelUserAttentionRequest:requestType];
        requestType = 0;
    }
}

void MacDock::overlay(const QString &text)
{
    [[NSApp dockTile] setBadgeLabel:(NSString *)QtCFString::toCFStringRef(text)];
}
