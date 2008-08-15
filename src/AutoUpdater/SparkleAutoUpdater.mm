/*
 * Copyright (C) 2008 Remko Troncon
 * See COPYING file for the detailed license.
 */

#include "SparkleAutoUpdater.h"

#include <Cocoa/Cocoa.h>
#include <Sparkle/Sparkle.h>

SparkleAutoUpdater::SparkleAutoUpdater(const QString& aUrl)
{
	SUUpdater* updater = [SUUpdater sharedUpdater];
	[updater retain];

	NSURL* url = [NSURL URLWithString:
			[NSString stringWithUTF8String: aUrl.toUtf8().data()]];
	[updater setFeedURL: url];
	updater_ = updater;
}

SparkleAutoUpdater::~SparkleAutoUpdater()
{
	[static_cast<SUUpdater*>(updater_) release];
	updater_ = 0;
}

void SparkleAutoUpdater::checkForUpdates()
{
	[static_cast<SUUpdater*>(updater_) checkForUpdatesInBackground];
}
