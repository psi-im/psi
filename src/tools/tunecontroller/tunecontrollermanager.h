/*
 * turencontrollermanager.h
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef TUNECONTROLLERMANAGER_H
#define TUNECONTROLLERMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QStringList>
#include <QSharedPointer>

#include "tune.h"

class TuneControllerPlugin;
class TuneController;

typedef QSharedPointer<TuneController> TuneControllerPtr;
typedef QSharedPointer<TuneControllerPlugin> TuneControllerPluginPtr;


class TuneControllerManager : public QObject
{
	Q_OBJECT
public:
	TuneControllerManager();
	~TuneControllerManager();

	QList<QString> controllerNames() const;
	TuneController* createController(const QString&) const;
	bool loadPlugin(const QString&);
	Tune currentTune() const;
	void setTuneFilters(const QStringList &filters, const QString &pattern);
	void updateControllers(const QStringList &blacklist);

signals:
	void playing(const Tune &tune);
	void stopped();

protected slots:
	void sendTune(const Tune &tune);

protected:
	bool loadPlugin(QObject* plugin);

private:
	bool checkTune(const Tune &tune) const;

private:
	QMap<QString,TuneControllerPluginPtr> plugins_;
	QMap<QString,TuneControllerPtr> controllers_;
	QStringList tuneUrlFilters_;
	QString tuneTitleFilterPattern_;
};

#endif
