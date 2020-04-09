/*
 * turencontrollerplugin.h
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

#ifndef TUNECONTROLLERPLUGIN_H
#define TUNECONTROLLERPLUGIN_H

#include <QPluginLoader>
#include <QString>

class TuneController;

/**
 * \brief Base class for TuneController plugins.
 */
class TuneControllerPlugin {
public:
    virtual ~TuneControllerPlugin() {}

    /**
     * \brief Returns the name of the tune controller.
     */
    virtual QString name() = 0;

    /**
     * \brief Creates a new controller.
     */
    virtual TuneController *createController() = 0;
};

Q_DECLARE_INTERFACE(TuneControllerPlugin, "be.el-tramo.TuneController/0.0-20060129");

#endif // TUNECONTROLLERPLUGIN_H
