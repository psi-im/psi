/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING file for the detailed license.
 */

#ifndef SPARKLEAUTOUPDATER_H
#define SPARKLEAUTOUPDATER_H

#include "AutoUpdater/AutoUpdater.h"

#include <QString>

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

#endif // SPARKLEAUTOUPDATER_H
