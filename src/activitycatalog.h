/*
 * activitycatalog.h
 * Copyright (C) 2008 Armando Jagucki
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef ACTIVITYCATALOG_H
#define ACTIVITYCATALOG_H

#include <QList>
#include <QObject>

#include "activity.h"

class QString;

class ActivityCatalog : public QObject
{
public:
	class Entry {
		public:
			Entry();
			Entry(Activity::Type, const QString&, const QString&);
			Entry(Activity::Type, Activity::SpecificType, const QString&, const QString&);
			Activity::Type type() const;
			Activity::SpecificType specificType() const;
			const QString& value() const;
			const QString& text() const;
			bool isNull() const;

		private:
			Activity::Type type_;
			Activity::SpecificType specificType_;
			QString value_;
			QString text_;
	};

	static ActivityCatalog* instance();

	Entry findEntryByType(Activity::Type) const;
	Entry findEntryByType(Activity::SpecificType) const;
	Entry findEntryByValue(const QString&) const;
	Entry findEntryByText(const QString& text) const;

	const QList<Entry>& entries() const;

private:
	ActivityCatalog();

	QList<Entry> entries_;
	static ActivityCatalog* instance_;

};

#endif
