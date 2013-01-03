/*
 * mprisplugin.cpp
 * Copyright (C) 2010 Vitaly Tonkacheyev
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include <QtCore>
#include <QObject>
#include <QString>

#include "mpristunecontroller.h"
#include "tunecontrollerplugin.h"

class MPRISPlugin : public QObject, public TuneControllerPlugin
{

	Q_OBJECT

	Q_INTERFACES(TuneControllerPlugin)
#ifdef HAVE_QT5
	Q_PLUGIN_METADATA(IID "org.psi-im.Psi.TuneControllerPlugin")
#endif
public:
	virtual QString name();
	virtual TuneController* createController();
};

#ifndef HAVE_QT5
Q_EXPORT_PLUGIN2(mprisplugin, MPRISPlugin);
#endif

QString MPRISPlugin::name()
{
	return "MPRIS";
}

TuneController* MPRISPlugin::createController()
{
	return new MPRISTuneController();
}

#include "mprisplugin.moc"
