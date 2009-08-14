/*
 * Copyright (C) 2008 Remko Troncon
 * See COPYING file for the detailed license.
 */

#include "CocoaUtilities/CocoaInitializer.h"

#include <AppKit/AppKit.h>
#include <Cocoa/Cocoa.h>
#include <QtDebug>

class CocoaInitializer::Private 
{
	public:
		NSAutoreleasePool* autoReleasePool_;
};

CocoaInitializer::CocoaInitializer()
{
#if 0
	d = new CocoaInitializer::Private();
	NSApplicationLoad();
	d->autoReleasePool_ = [[NSAutoreleasePool alloc] init];
#endif
}

CocoaInitializer::~CocoaInitializer()
{
#if 0
	[d->autoReleasePool_ release];
	delete d;
#endif
}
