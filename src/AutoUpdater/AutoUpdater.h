/*
 * Copyright (C) 2008 Remko Troncon
 * See COPYING file for the detailed license.
 */

#ifndef AUTOUPDATER_H
#define AUTOUPDATER_H

class AutoUpdater
{
	public:
		virtual ~AutoUpdater();

		virtual void checkForUpdates() = 0;
};

#endif
