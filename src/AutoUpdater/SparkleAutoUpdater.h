/*
 * Copyright (C) 2008 Remko Troncon
 * See COPYING file for the detailed license.
 */

#ifndef SPARKLEAUTOUPDATER_H
#define SPARKLEAUTOUPDATER_H

#include <QString>

#include "AutoUpdater/AutoUpdater.h"

class SparkleAutoUpdater : public AutoUpdater
{
	public:
		SparkleAutoUpdater(const QString& url);
		~SparkleAutoUpdater();

		void checkForUpdates();
	
	private:
		class Private;
		Private* d;
};

#endif
