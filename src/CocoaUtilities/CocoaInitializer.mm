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
	d = new CocoaInitializer::Private();
	NSApplicationLoad();
	d->autoReleasePool_ = [[NSAutoreleasePool alloc] init];
}

CocoaInitializer::~CocoaInitializer()
{
	[d->autoReleasePool_ release];
	delete d;
}
