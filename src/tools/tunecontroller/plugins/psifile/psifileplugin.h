/*
 * psifileplugin.h
 * Copyright (C) 2006  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PSIFILEPLUGIN_H
#define PSIFILEPLUGIN_H

#ifndef QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "filetunecontroller.h"
#include "tunecontrollerplugin.h"

#include <QObject>
#include <QString>
#include <QtCore>

/**
 * \brief A TuneController plugin for the Psi file controller.
 */
class PsiFilePlugin : public QObject, public TuneControllerPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.psi-im.Psi.TuneControllerPlugin")
    Q_INTERFACES(TuneControllerPlugin)

public:
    virtual QString         name();
    virtual TuneController *createController();
};

#endif // PSIFILEPLUGIN_H
